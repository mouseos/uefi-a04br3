/** @file
  Benesse TAB-A04-BR3 persistent boot log reader (UniXNU log-capture guide, "persistent logs").

  On ReadyToBoot, finds the eMMC user area by its content (a GPT with a "userdata" entry; the SD card has none and the boot
  ramdisk has no GPT), and the boot log window in the last 8 MiB of userdata, written by the previous kernels
  (UniXNU-a04br3 package/A04BR3BootLog.c with NativeMsdc native-msdc-log=1): 8 slots of 1 MiB, each a 4 KiB header block
  and the kernel message bytes. The newest slot (boot counter, then sequence) whose header and log CRC verify is printed on
  the serial port as
     A04BLOG-BEGIN <128-byte header in hex>
     A04BLOG <base64, 57 bytes per line>
     A04BLOG-END
  or "A04BLOG-NONE <reason>", for the host decoder (UniXNU-a04br3 scripts/read-a04br3-log.py). Read-only: nothing is
  written to the eMMC. Built only with -D A04BR3_LOG_READER=1 (a04br3.dsc, Include/DXE.inc).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>

#define LOG_WINDOW_SECTORS  (2048u * 8u)          // 8 MiB of 512-byte sectors (A04BR3BootLog.c, NativeMsdc.h logWindow)
#define LOG_SLOTS           8u
#define LOG_SLOT_SECTORS    (LOG_WINDOW_SECTORS / LOG_SLOTS)
#define LOG_MAX_BYTES       (512u * 1024u)
#define LOG_HEADER_BYTES    128u

STATIC CONST CHAR8 mBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
STATIC UINT32      mCrcTable[256];

STATIC VOID
CrcInit (VOID)
{
  for (UINT32 N = 0; N < 256; N++) {
    UINT32 C = N;
    for (UINTN Bit = 0; Bit < 8; Bit++) {
      C = (C >> 1) ^ (0xEDB88320u & (0u - (C & 1u)));
    }
    mCrcTable[N] = C;
  }
}

STATIC UINT32
LogCrc32 (CONST UINT8 *Data, UINTN Length)
{
  UINT32 Crc = 0xFFFFFFFFu;
  for (UINTN Index = 0; Index < Length; Index++) {
    Crc = mCrcTable[(Crc ^ Data[Index]) & 0xFF] ^ (Crc >> 8);
  }
  return ~Crc;
}

STATIC VOID
Emit (CONST CHAR8 *Text)
{
  SerialPortWrite ((UINT8 *)Text, AsciiStrLen (Text));
}

STATIC BOOLEAN
NameIsUserdata (CONST UINT8 *Name)
{
  STATIC CONST CHAR8 Wanted[] = "userdata";
  for (UINTN Index = 0; Index < sizeof (Wanted); Index++) {   // includes the terminating zero
    if (Name[2 * Index] != (UINT8)Wanted[Index] || Name[2 * Index + 1] != 0) {
      return FALSE;
    }
  }
  return TRUE;
}

/** Finds the userdata entry of the primary GPT; returns its last LBA or 0. Each entry sector is read once. */
STATIC EFI_LBA
UserdataLastLba (EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT8 *Sector)
{
  if (BlockIo->Media->BlockSize != 512 || BlockIo->Media->LastBlock < 34 ||
      EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, 1, 512, Sector)) ||
      CompareMem (Sector, "EFI PART", 8) != 0) {
    return 0;
  }
  UINT64 EntriesLba = *(UINT64 *)(Sector + 72);
  UINT32 Count      = *(UINT32 *)(Sector + 80);
  UINT32 EntrySize  = *(UINT32 *)(Sector + 84);
  if (EntrySize != 128 || Count == 0 || Count > 256 || EntriesLba < 2 || EntriesLba + Count / 4 > BlockIo->Media->LastBlock) {
    return 0;
  }
  for (UINT32 SectorIndex = 0; SectorIndex < (Count + 3) / 4; SectorIndex++) {
    if (EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, EntriesLba + SectorIndex, 512, Sector))) {
      return 0;
    }
    for (UINT32 Index = 0; Index < 4 && SectorIndex * 4 + Index < Count; Index++) {
      CONST UINT8 *Entry = Sector + Index * 128;
      if (NameIsUserdata (Entry + 56)) {
        UINT64 First = *(UINT64 *)(Entry + 32), Last = *(UINT64 *)(Entry + 40);
        return (Last > First + LOG_WINDOW_SECTORS && Last <= BlockIo->Media->LastBlock) ? Last : 0;
      }
    }
  }
  return 0;
}

STATIC BOOLEAN
HeaderValid (CONST UINT8 *Header)
{
  return CompareMem (Header, "A04BLOG1", 8) == 0 && *(CONST UINT32 *)(Header + 8) == 2 &&
         *(CONST UINT32 *)(Header + 12) == LOG_HEADER_BYTES && LogCrc32 (Header, 124) == *(CONST UINT32 *)(Header + 124) &&
         *(CONST UINT32 *)(Header + 40) <= LOG_MAX_BYTES;
}

STATIC VOID
EmitSlot (CONST UINT8 *Header, CONST UINT8 *Data, UINT32 LogBytes)
{
  CHAR8 Line[96];
  Emit ("A04BLOG-BEGIN ");
  for (UINTN Index = 0; Index < LOG_HEADER_BYTES; Index++) {
    AsciiSPrint (Line, sizeof (Line), "%02x", Header[Index]);
    Emit (Line);
  }
  Emit ("\r\n");
  for (UINT32 Offset = 0; Offset < LogBytes; Offset += 57) {
    UINT32 Chunk = (LogBytes - Offset) < 57 ? (LogBytes - Offset) : 57;
    UINTN  Out   = 0;
    CopyMem (Line, "A04BLOG ", 8);
    Out = 8;
    for (UINT32 Index = 0; Index < Chunk; Index += 3) {
      UINT32 Value = (UINT32)Data[Offset + Index] << 16;
      if (Index + 1 < Chunk) { Value |= (UINT32)Data[Offset + Index + 1] << 8; }
      if (Index + 2 < Chunk) { Value |= Data[Offset + Index + 2]; }
      Line[Out++] = mBase64[(Value >> 18) & 63];
      Line[Out++] = mBase64[(Value >> 12) & 63];
      Line[Out++] = (Index + 1 < Chunk) ? mBase64[(Value >> 6) & 63] : '=';
      Line[Out++] = (Index + 2 < Chunk) ? mBase64[Value & 63] : '=';
    }
    Line[Out++] = '\r'; Line[Out++] = '\n'; Line[Out] = 0;
    Emit (Line);
  }
  Emit ("A04BLOG-END\r\n");
}

/** Prints the newest verified slot of the window that ends at Last. */
STATIC VOID
DumpWindow (EFI_BLOCK_IO_PROTOCOL *BlockIo, EFI_LBA Last, UINT8 *Buffer)
{
  EFI_LBA First = Last + 1 - LOG_WINDOW_SECTORS;
  UINT8   Headers[LOG_SLOTS][LOG_HEADER_BYTES];
  BOOLEAN Valid[LOG_SLOTS];
  for (UINT32 Slot = 0; Slot < LOG_SLOTS; Slot++) {
    Valid[Slot] = !EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, First + Slot * LOG_SLOT_SECTORS, 512, Buffer)) &&
                  HeaderValid (Buffer);
    CopyMem (Headers[Slot], Buffer, LOG_HEADER_BYTES);
  }
  // Newest first: try the slots in descending (boot counter, sequence) order until one's log CRC verifies.
  for (UINT32 Round = 0; Round < LOG_SLOTS; Round++) {
    INTN Best = -1;
    for (UINT32 Slot = 0; Slot < LOG_SLOTS; Slot++) {
      if (!Valid[Slot]) { continue; }
      if (Best < 0) { Best = Slot; continue; }
      UINT64 C = *(UINT64 *)(Headers[Slot] + 16), S = *(UINT64 *)(Headers[Slot] + 24);
      UINT64 BC = *(UINT64 *)(Headers[Best] + 16), BS = *(UINT64 *)(Headers[Best] + 24);
      if (C > BC || (C == BC && S > BS)) { Best = Slot; }
    }
    if (Best < 0) {
      break;
    }
    Valid[Best] = FALSE;
    UINT32 LogBytes = *(UINT32 *)(Headers[Best] + 40);
    UINT32 Sectors  = (LogBytes + 511) / 512;
    if (Sectors != 0 &&
        EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, First + Best * LOG_SLOT_SECTORS + 8, Sectors * 512, Buffer))) {
      continue;
    }
    if (LogCrc32 (Buffer, LogBytes) != *(UINT32 *)(Headers[Best] + 48)) {
      Emit ("A04BLOG-NOTE slot skipped: log CRC mismatch\r\n");
      continue;
    }
    EmitSlot (Headers[Best], Buffer, LogBytes);
    return;
  }
  Emit ("A04BLOG-NONE no verified slot\r\n");
}

STATIC VOID EFIAPI
OnReadyToBoot (EFI_EVENT Event, VOID *Context)
{
  gBS->CloseEvent (Event);
  UINTN      Count   = 0;
  EFI_HANDLE *Handles = NULL;
  UINT8      *Sector = AllocatePool (512);
  UINT8      *Buffer = AllocatePool (LOG_MAX_BYTES);
  if (Sector == NULL || Buffer == NULL ||
      EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles))) {
    Emit ("A04BLOG-NONE no block devices\r\n");
    goto Done;
  }
  CrcInit ();
  for (UINTN Index = 0; Index < Count; Index++) {
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo)) ||
        BlockIo->Media->LogicalPartition || !BlockIo->Media->MediaPresent || BlockIo->Media->RemovableMedia) {
      continue;   // whole fixed devices only; the eMMC is the one whose GPT has a userdata entry
    }
    EFI_LBA Last = UserdataLastLba (BlockIo, Sector);
    if (Last != 0) {
      DumpWindow (BlockIo, Last, Buffer);
      goto Done;
    }
  }
  Emit ("A04BLOG-NONE no emmc userdata\r\n");
Done:
  if (Handles != NULL) { FreePool (Handles); }
  if (Sector != NULL) { FreePool (Sector); }
  if (Buffer != NULL) { FreePool (Buffer); }
}

EFI_STATUS
EFIAPI
A04BR3BootLogReaderDxeEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;
  return gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnReadyToBoot, NULL, &gEfiEventReadyToBootGuid, &Event);
}

