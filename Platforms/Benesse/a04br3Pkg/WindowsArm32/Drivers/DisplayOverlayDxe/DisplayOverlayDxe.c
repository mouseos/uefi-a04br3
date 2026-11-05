/** Remove LK's warning overlay from the Windows GOP scanout.
  The board display initializer and other operating systems retain their own
  layer policy. This Windows-only DXE waits until GOP is installed and changes
  only the enable bit for layer 3; it leaves the main layer and pixels intact.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiDriverEntryPoint.h>

#define A04_OVL_SRC_CON  0x1400702cU
#define A04_MAIN_LAYER   BIT0
#define A04_LK_LAYER     BIT3

EFI_STATUS EFIAPI
A04DisplayOverlayEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  UINT32 Before, After;
  (VOID)ImageHandle;
  (VOID)SystemTable;
  Before = MmioRead32 (A04_OVL_SRC_CON);
  if ((Before & A04_MAIN_LAYER) == 0) {
    DEBUG ((DEBUG_ERROR, "A04DISPLAY overlay unchanged: main layer inactive (%08x)\n", Before));
    return EFI_NOT_READY;
  }
  MmioWrite32 (A04_OVL_SRC_CON, Before & ~A04_LK_LAYER);
  After = MmioRead32 (A04_OVL_SRC_CON);
  DEBUG ((DEBUG_ERROR, "A04DISPLAY LK overlay enable %08x -> %08x\n", Before, After));
  return ((After & (A04_MAIN_LAYER | A04_LK_LAYER)) == A04_MAIN_LAYER) ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}
