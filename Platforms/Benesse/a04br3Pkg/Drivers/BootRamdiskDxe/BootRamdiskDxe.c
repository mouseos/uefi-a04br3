/** @file
  Boot image ramdisk of TAB-A04-BR3 as a read-only Block I/O device.

  UniXNU ships its payload (configuration, kernel, ADT, trust cache) as a FAT
  volume in the ramdisk of the Android boot image.

  What the stock A04BR3 LK (CT3-01.06.000, unixnu/_research/lk/lk.elf) does
  with the ramdisk, from REA of its boot path:
   - FUN_41e03ea4 reads the boot image and checks the ramdisk for a MediaTek
     "ROOTFS" header (FUN_41e03880: magic 0x58881688, name at +8, payload
     size at +4). When present it moves the ramdisk pointer past the 512-byte
     header and takes the size from the header ("mkimg header exist in
     ramdisk image"); otherwise it uses the boot image ramdisk_size.
   - FUN_41e1b804 copies that payload to the ramdisk_addr of the boot image
     header (offset 0x14). Only without a boot image header does it fall back
     to 0x44000000, the ramdisk_addr of the stock Android boot image.
   - FUN_41e1a804 writes /chosen linux,initrd-start = ramdisk_addr and
     linux,initrd-end = ramdisk_addr + payload size, copies the device tree
     to tags_addr (0x4E000000) and enters the kernel slot with it in x0.
  The a04br3 boot image (Resources/Configs/a04br3.toml) has base 0x40000000
  and ramdisk_offset 0x20000000, so the payload lies at 0x60000000, the
  start of the "Boot Ramdisk" region of MemoryMapLib.

  SEC keeps x0 in the gSiliciumEntryArgHobGuid HOB. This driver takes the
  initrd range from /chosen of that device tree, the address the loader
  actually used, and publishes it as a read-only, 512-byte-sector Block I/O
  device, so Partition/Fat drivers mount the payload volume. If the range
  still starts with a MediaTek header (a loader that does not strip it), the
  512-byte header is skipped.

  Both the device tree and the initrd must lie inside the "LK DTB" and
  "Boot Ramdisk" regions of the a04br3 memory map; these are reserved, so no
  firmware allocation can overwrite them. A range elsewhere is not published:
  firmware allocations may already have overwritten it.

  Derived from UniXNU/uefi-sheng shengPkg ShengBootMemoryDxe (initrd part).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#define RAM_BLOCK_SIZE  512

//
// MediaTek image header in front of the boot image ramdisk: checked by LK
// FUN_41e03880, written by build_uefi.py mtk_image_section (magic, payload
// size, 32-byte name "ROOTFS", 0xFF padding to 512 bytes).
//
#define MTK_HEADER_MAGIC  0x58881688U
#define MTK_HEADER_SIZE   512

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  EFI_DEVICE_PATH_PROTOCOL    End;
} RAM_BLOCK_DEVICE_PATH;
#pragma pack()

STATIC RAM_BLOCK_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { (UINT8)sizeof (VENDOR_DEVICE_PATH), (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8) } },
    { 0x5D3A0B2C, 0x8167, 0x4A04, { 0xB3, 0x0D, 0x52, 0x41, 0x4D, 0x44, 0x49, 0x53 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC UINT8                    *mRamBase;
STATIC EFI_BLOCK_IO_MEDIA       mMedia;

STATIC
UINT64
ReadCells (
  IN CONST VOID  *Data,
  IN UINT32      Cells
  )
{
  if (Cells == 2) {
    return Fdt64ToCpu (ReadUnaligned64 (Data));
  }

  return Fdt32ToCpu (ReadUnaligned32 (Data));
}

STATIC
BOOLEAN
InsideRegion (
  IN CHAR8   *RegionName,
  IN UINT64  Base,
  IN UINT64  Size
  )
{
  EFI_MEMORY_REGION_DESCRIPTOR  Region;

  if (EFI_ERROR (LocateMemoryRegionByName (RegionName, &Region))) {
    return FALSE;
  }

  return (Size != 0) && (Base >= Region.Address) && (Base + Size > Base) &&
         (Base + Size <= Region.Address + Region.Length);
}

STATIC
EFI_STATUS
EFIAPI
RamBlockReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockRead (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (MediaId != mMedia.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if ((BufferSize % RAM_BLOCK_SIZE) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Lba > mMedia.LastBlock) || ((BufferSize / RAM_BLOCK_SIZE) > (mMedia.LastBlock + 1 - Lba))) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mRamBase + Lba * RAM_BLOCK_SIZE, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockWrite (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo = {
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  &mMedia,
  RamBlockReset,
  RamBlockRead,
  RamBlockWrite,
  RamBlockFlush
};

STATIC
VOID
PublishInitrd (
  IN CONST VOID  *Fdt
  )
{
  EFI_STATUS  Status;
  INT32       Chosen;
  CONST VOID  *StartProp;
  CONST VOID  *EndProp;
  INT32       StartLength;
  INT32       EndLength;
  UINT64      Start;
  UINT64      End;
  EFI_HANDLE  Handle;

  Chosen = FdtPathOffset (Fdt, "/chosen");
  if (Chosen < 0) {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: no /chosen\n"));
    return;
  }

  StartProp = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
  EndProp   = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
  if ((StartProp == NULL) || (EndProp == NULL) ||
      ((StartLength != 4) && (StartLength != 8)) || ((EndLength != 4) && (EndLength != 8)))
  {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: no initrd in /chosen\n"));
    return;
  }

  Start = ReadCells (StartProp, StartLength / 4);
  End   = ReadCells (EndProp, EndLength / 4);
  if ((End <= Start) || !InsideRegion ("Boot Ramdisk", Start, End - Start)) {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: initrd 0x%lx-0x%lx is outside the reserved \"Boot Ramdisk\" region\n", Start, End));
    return;
  }

  // The stock LK already strips the header; skip one left in place.
  if ((End - Start > MTK_HEADER_SIZE) &&
      (ReadUnaligned32 ((VOID *)(UINTN)Start) == MTK_HEADER_MAGIC))
  {
    UINT32  PayloadSize;

    PayloadSize = ReadUnaligned32 ((VOID *)(UINTN)(Start + 4));
    DEBUG ((DEBUG_ERROR, "A04BR3RD: skipping MediaTek header at 0x%lx (payload 0x%x bytes)\n", Start, PayloadSize));
    Start += MTK_HEADER_SIZE;
    if ((PayloadSize != 0) && (PayloadSize < End - Start)) {
      End = Start + PayloadSize;
    }
  }

  if (((End - Start) % RAM_BLOCK_SIZE) != 0) {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: initrd 0x%lx-0x%lx is not a whole number of sectors\n", Start, End));
    return;
  }

  mRamBase                = (UINT8 *)(UINTN)Start;
  mMedia.MediaId          = 1;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = RAM_BLOCK_SIZE;
  mMedia.IoAlign          = 1;
  mMedia.LastBlock        = (End - Start) / RAM_BLOCK_SIZE - 1;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  &mDevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &mBlockIo,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "A04BR3RD: initrd block device 0x%lx size 0x%lx %r\n", Start, End - Start, Status));
}

EFI_STATUS
EFIAPI
BootRamdiskDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HOB_GUID_TYPE  *Hob;
  UINT64             Dtb;

  Hob = GetFirstGuidHob (&gSiliciumEntryArgHobGuid);
  if (Hob == NULL) {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: no entry argument HOB\n"));
    return EFI_SUCCESS;
  }

  Dtb = *(UINT64 *)GET_GUID_HOB_DATA (Hob);

  // Only touch x0 when it points into the reserved LK DTB window.
  if ((Dtb & 7) != 0 || !InsideRegion ("LK DTB", Dtb, sizeof (FDT_HEADER))) {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: x0 0x%lx is not in the \"LK DTB\" region\n", Dtb));
    return EFI_SUCCESS;
  }

  if ((FdtCheckHeader ((VOID *)(UINTN)Dtb) != 0) ||
      !InsideRegion ("LK DTB", Dtb, FdtTotalSize ((VOID *)(UINTN)Dtb)))
  {
    DEBUG ((DEBUG_ERROR, "A04BR3RD: x0 0x%lx is not a device tree\n", Dtb));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, "A04BR3RD: device tree at 0x%lx, %u bytes\n", Dtb, (UINT32)FdtTotalSize ((VOID *)(UINTN)Dtb)));

  PublishInitrd ((VOID *)(UINTN)Dtb);
  return EFI_SUCCESS;
}
