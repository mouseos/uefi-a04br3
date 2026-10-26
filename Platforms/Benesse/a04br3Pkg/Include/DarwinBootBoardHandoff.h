// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
// Optional board hook over the FINAL owned ADT copy, before GetMemoryMap/EBS.
// Attach must not allocate, resize, retain external allocations, or change tree
// structure. Board-owned inline metadata may be finalized by its EBS callback.
#define DARWIN_BOOT_BOARD_HANDOFF_GUID {0xad9f1634,0xd315,0x4731,{0x8e,0xda,0x2d,0x37,0xb4,0x65,0x16,0x23}}
typedef struct {
  UINT32 Version;
  EFI_STATUS (EFIAPI *AttachTree)(VOID *Tree, UINTN TreeBytes);
} DARWIN_BOOT_BOARD_HANDOFF;
