/** @file
  Benesse TAB-A04-BR3 persistent boot log reader (UniXNU log-capture guide, "persistent logs").

  On ReadyToBoot, finds the eMMC user area (EmmcBlockIoDxe), the GPT entry named "userdata" and the boot log window in its
  last 8 MiB, written by the previous kernel (UniXNU-a04br3 package/A04BR3BootLog.c with NativeMsdc native-msdc-log=1).
  A window whose header passes its magic, version and CRC checks is printed on the serial port as
     A04BLOG-BEGIN <64-byte header in hex>
     A04BLOG <base64, 57 bytes per line>
     A04BLOG-END
  for the host decoder (UniXNU-a04br3 scripts/read-a04br3-log.py), which also checks the log CRC. Read-only: nothing is
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
#define LOG_MAX_BYTES       (512u * 1024u)
#define LOG_HEADER_BYTES    64u

STATIC CONST CHAR8 mBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

STATIC UINT32
LogCrc32 (CONST UINT8 *Data, UINTN Length)
{
  UINT32 Crc = 0xFFFFFFFFu;
  for (UINTN Index = 0; Index < Length; Index++) {
    Crc ^= Data[Index];
    for (UINTN Bit = 0; Bit < 8; Bit++) {
      Crc = (Crc >> 1) ^ (0xEDB88320u & (0u - (Crc & 1u)));
    }
  }
  return ~Crc;
}

STATIC VOID
Emit (CONST CHAR8 *Text)
{
  SerialPortWrite ((UINT8 *)Text, AsciiStrLen (Text));
}

STATIC BOOLEAN
NameIsUserdata (CONST CHAR16 *Name)
{
  STATIC CONST CHAR16 Wanted[] = L"userdata";
  for (UINTN Index = 0; Index < ARRAY_SIZE (Wanted); Index++) {
    if (Name[Index] != Wanted[Index]) {
      return FALSE;
    }
  }
  return TRUE;
}

/** Finds the userdata entry of the primary GPT; returns its last LBA or 0. */
STATIC EFI_LBA
UserdataLastLba (EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT8 *Sector)
{
  if (EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, 1, 512, Sector)) ||
      CompareMem (Sector, "EFI PART", 8) != 0) {
    return 0;
  }
  UINT64 EntriesLba = *(UINT64 *)(Sector + 72);
  UINT32 Count      = *(UINT32 *)(Sector + 80);
  UINT32 EntrySize  = *(UINT32 *)(Sector + 84);
  if (EntrySize < 128 || EntrySize > 512 || Count > 256 || EntriesLba < 2) {
    return 0;
  }
  for (UINT32 Index = 0; Index < Count; Index++) {
    UINT64 Byte = (UINT64)Index * EntrySize;
    if (EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, EntriesLba + Byte / 512, 512, Sector))) {
      return 0;
    }
    CONST UINT8 *Entry = Sector + (Byte % 512);
    if (Byte % 512 + 128 > 512) {
      continue;   // the reader handles entries that do not straddle sectors (128-byte entries never do)
    }
    if (NameIsUserdata ((CONST CHAR16 *)(Entry + 56))) {
      UINT64 First = *(UINT64 *)(Entry + 32), Last = *(UINT64 *)(Entry + 40);
      return (Last > First + LOG_WINDOW_SECTORS && Last <= BlockIo->Media->LastBlock) ? Last : 0;
    }
  }
  return 0;
}

STATIC VOID
DumpWindow (EFI_BLOCK_IO_PROTOCOL *BlockIo)
{
  UINT8 *Sector = AllocatePool (512);
  UINT8 *Buffer = AllocatePool (4096 + LOG_MAX_BYTES);
  if (Sector == NULL || Buffer == NULL) {
    goto Done;
  }
  EFI_LBA Last = UserdataLastLba (BlockIo, Sector);
  if (Last == 0) {
    Emit ("A04BLOG-NONE no userdata\r\n");
    goto Done;
  }
  EFI_LBA First = Last + 1 - LOG_WINDOW_SECTORS;
  if (EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, First, 4096, Buffer))) {
    Emit ("A04BLOG-NONE read error\r\n");
    goto Done;
  }
  UINT32 LogBytes = *(UINT32 *)(Buffer + 40);
  if (CompareMem (Buffer, "A04BLOG1", 8) != 0 || *(UINT32 *)(Buffer + 8) != 1 || *(UINT32 *)(Buffer + 12) != LOG_HEADER_BYTES ||
      LogCrc32 (Buffer, 60) != *(UINT32 *)(Buffer + 60) || LogBytes > LOG_MAX_BYTES) {
    Emit ("A04BLOG-NONE no valid header\r\n");
    goto Done;
  }
  UINT32 Sectors = (LogBytes + 511) / 512;
  if (Sectors != 0 &&
      EFI_ERROR (BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, First + 8, Sectors * 512, Buffer + 4096))) {
    Emit ("A04BLOG-NONE read error\r\n");
    goto Done;
  }
  CHAR8 Line[96];
  Emit ("A04BLOG-BEGIN ");
  for (UINTN Index = 0; Index < LOG_HEADER_BYTES; Index++) {
    AsciiSPrint (Line, sizeof (Line), "%02x", Buffer[Index]);
    Emit (Line);
  }
  Emit ("\r\n");
  CONST UINT8 *Data = Buffer + 4096;
  for (UINT32 Offset = 0; Offset < LogBytes; Offset += 57) {
    UINT32 Chunk = (LogBytes - Offset) < 57 ? (LogBytes - Offset) : 57;
    UINTN  Out   = 0;
    Line[Out++] = 'A'; Line[Out++] = '0'; Line[Out++] = '4'; Line[Out++] = 'B'; Line[Out++] = 'L';
    Line[Out++] = 'O'; Line[Out++] = 'G'; Line[Out++] = ' ';
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
Done:
  if (Sector != NULL) { FreePool (Sector); }
  if (Buffer != NULL) { FreePool (Buffer); }
}

STATIC VOID EFIAPI
OnReadyToBoot (EFI_EVENT Event, VOID *Context)
{
  gBS->CloseEvent (Event);
  UINTN       Count   = 0;
  EFI_HANDLE  *Handles = NULL;
  if (EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles))) {
    return;
  }
  for (UINTN Index = 0; Index < Count; Index++) {
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo)) ||
        BlockIo->Media->LogicalPartition || !BlockIo->Media->MediaPresent || BlockIo->Media->BlockSize != 512 ||
        BlockIo->Media->RemovableMedia || BlockIo->Media->ReadOnly) {
      continue;   // the eMMC user area: whole device, fixed, writable media (the boot ramdisk is read-only)
    }
    DumpWindow (BlockIo);
    break;
  }
  FreePool (Handles);
}

EFI_STATUS
EFIAPI
A04BR3BootLogReaderDxeEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;
  return gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnReadyToBoot, NULL, &gEfiEventReadyToBootGuid, &Event);
}

