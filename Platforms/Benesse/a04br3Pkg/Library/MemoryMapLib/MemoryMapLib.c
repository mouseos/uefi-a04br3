/**
  Benesse TAB-A04-BR3 (MT8167) memory map.

  DRAM layout follows the working edk2-mtk A04BR3 map
  (Platform/Benesse/A04BR3Pkg/Library/PlatformMemoryMapLib, commit
  1982d888c765), the A04BR3 DT reservations and the boot image layout of the
  A04BR3 XNU device repository (tools/bootimg.py, board.json boot_image):

    0x40080000  kernel load address (base 0x40000000 + kernel_offset 0x80000).
                LK decompresses BootShim + FD here; BootShim copies the FD to
                FD_BASE (0x41080000, as edk2-mtk A04BR3).
    0x43000000  ATF reservation, 0x30000 bytes (DT atf-reserved-memory).
    0x44400000  ram_console, pstore and minirdump (DT reserved-memory).
    0x4E000000  DTB copied by LK (tags_addr; ATF passes it in x0).
    0x80000000  boot image ramdisk (ramdisk_addr of the XNU device repository,
                which carries payloads up to the 128 MiB boot partition).
                With UniXNU this is the payload FAT volume.
    0xBE9C0000  LK frame buffer, 16 MiB (DT framebuffer@be9c0000).
    0xBF9C0000  MBlock gap up to 0xC0000000 (edk2-mtk A04BR3).

  Register windows cover every block the ported drivers touch.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryMapLib.h>

#define A04BR3_BOOT_PAYLOAD_BASE  0x40080000ULL
#define A04BR3_ATF_RESERVED_BASE  0x43000000ULL

STATIC
EFI_MEMORY_REGION_DESCRIPTOR
gMemoryDescriptor[] = {
  // Name, Address, Length, HobOption, ResourceType, ResourceAttribute, MemoryType, ArmAttribute

  // DDR Regions
  {"DDR Low",            0x40000000, 0x00080000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  {"Boot Payload",       0x40080000, 0x01000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // Fixed up from FD_BASE
  {"UEFI FD",            0x41080000, 0x00300000, AddMem, SYS_MEM, SYS_MEM_CAP, BsCode, WRITE_BACK},    // Fixed up from FD_BASE/FD_SIZE
  {"DDR Before ATF",     0x41380000, 0x01C80000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // Fixed up from FD_BASE/FD_SIZE
  {"ATF Reserved",       0x43000000, 0x00030000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"DDR Mid 0",          0x43030000, 0x013D0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  {"RAM Console",        0x44400000, 0x00010000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"PStore",             0x44410000, 0x000E0000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"MiniRdDump",         0x444F0000, 0x00010000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"DDR Mid 1",          0x44500000, 0x09B00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  {"LK DTB",             0x4E000000, 0x00200000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"UEFI Stack",         0x4E200000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"DXE Heap",           0x4E240000, 0x03DC0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK},
  {"DDR Mid 2",          0x52000000, 0x2E000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  {"Boot Ramdisk",       0x80000000, 0x08000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"DDR High",           0x88000000, 0x369C0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  {"Display Reserved",   0xBE9C0000, 0x01000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},
  {"MBlock Gap",         0xBF9C0000, 0x00640000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // Register Regions (A04BR3 DT and edk2-mtk A04BR3 drivers)
  {"Top Ck Gen",         0x10000000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Infra Cfg AO",       0x10001000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Keypad",             0x10002000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Pinctrl",            0x10005000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"SPM",                0x10006000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"WatchDog Timer",     0x10007000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"PMIC Wrapper",       0x1000F000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"GIC Distributor",    0x10310000, 0x00010000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"GIC CPU Interface",  0x10320000, 0x00020000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"UART0",              0x11005000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC-0",             0x11120000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"MSDC-1",             0x11130000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Display MMSYS",      0x14000000, 0x00020000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}
};

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount)
{
  EFI_PHYSICAL_ADDRESS FdBase = FixedPcdGet64 (PcdFdBaseAddress);
  UINT64               FdEnd  = FdBase + FixedPcdGet32 (PcdFdSize);

  // The FD window comes from Resources/Configs/a04br3.toml ([uefi_fd]); it
  // must lie between the boot payload and the ATF reservation.
  ASSERT (FdBase > A04BR3_BOOT_PAYLOAD_BASE);
  ASSERT (FdEnd < A04BR3_ATF_RESERVED_BASE);

  for (UINTN Index = 0; Index < ARRAY_SIZE (gMemoryDescriptor); Index++) {
    EFI_MEMORY_REGION_DESCRIPTOR *Region = &gMemoryDescriptor[Index];

    if (AsciiStrCmp (Region->Name, "Boot Payload") == 0) {
      Region->Length = FdBase - A04BR3_BOOT_PAYLOAD_BASE;
    } else if (AsciiStrCmp (Region->Name, "UEFI FD") == 0) {
      Region->Address = FdBase;
      Region->Length  = FixedPcdGet32 (PcdFdSize);
    } else if (AsciiStrCmp (Region->Name, "DDR Before ATF") == 0) {
      Region->Address = FdEnd;
      Region->Length  = A04BR3_ATF_RESERVED_BASE - FdEnd;
    }
  }

  // Pass Data
  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = ARRAY_SIZE (gMemoryDescriptor);
}
