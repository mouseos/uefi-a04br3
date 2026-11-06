/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Optional MT8167 PowerVR GE8300 power handoff. The sequencing is derived
 * from UniXNU-a04br3's A04BR3Gpu.cpp and A04BR3RoguePower.h. It runs before
 * ExitBootServices, before Windows owns the shared clock and power registers.
 * This is not a WDDM driver and does not load GPU firmware or submit work. */
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>
#include "rogue_power.h"

STATIC CONST UINTN Banks[] = { 0x10006000U, 0x10001000U, 0x10000000U, 0x13ffe000U };
STATIC BOOLEAN Attempted;
STATIC UINT32 ReadBank(VOID *Context, unsigned Bank, unsigned Offset) {
  (VOID)Context;
  return MmioRead32(Banks[Bank] + Offset);
}
STATIC VOID WriteBank(VOID *Context, unsigned Bank, unsigned Offset, UINT32 Value) {
  (VOID)Context;
  MmioWrite32(Banks[Bank] + Offset, Value);
}
STATIC VOID DelayUs(VOID *Context, unsigned Usec) {
  (VOID)Context;
  MicroSecondDelay(Usec);
}
STATIC VOID EFIAPI GpuBeforeExit(EFI_EVENT Event, VOID *Context) {
  struct a04_rgx_io Io = { NULL, ReadBank, WriteBank, DelayUs };
  UINT32 Low, High, Revision, PolarityBefore, PolarityAfter;
  int Result;
  (VOID)Event;
  (VOID)Context;
  if (Attempted) return;
  Attempted = TRUE;
  /* MT8167 Linux routes GPU SPI185 through the sysirq polarity inverter.
   * irq-mtk-sysirq.c maps LEVEL_LOW to GIC LEVEL_HIGH and sets bit185.
   * Establish that SoC-specific stage before Windows' generic GIC owns IRQ217.
   * Preserve every unrelated interrupt-polarity bit. */
  PolarityBefore = MmioRead32(0x10200634U);
  MmioWrite32(0x10200634U, PolarityBefore | BIT25);
  PolarityAfter = MmioRead32(0x10200634U);
  DEBUG((DEBUG_ERROR, "A04GPU SYSIRQ185 polarity before=%08x after=%08x\n",
    PolarityBefore, PolarityAfter));
  if ((PolarityAfter & BIT25) == 0) {
    DEBUG((DEBUG_ERROR, "A04GPU SYSIRQ185 polarity unavailable\n"));
    return;
  }
  Result = a04_rgx_power_on(&Io);
  DEBUG((DEBUG_ERROR, "A04GPU power result=%d\n", Result));
  if (Result != 0) return;
  /* Power-domain acknowledgements precede any access to the GPU core. */
  Low = MmioRead32(0x13000020U);
  High = MmioRead32(0x13000024U);
  Revision = MmioRead32(0x13000018U);
  DEBUG((DEBUG_ERROR, "A04GPU BVNC=%08x%08x revision=%08x\n", High, Low, Revision));
}
EFI_STATUS EFIAPI GpuPowerEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_EVENT Event;
  (VOID)ImageHandle;
  (VOID)SystemTable;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, GpuBeforeExit, NULL,
                            &gEfiEventBeforeExitBootServicesGuid, &Event);
}
