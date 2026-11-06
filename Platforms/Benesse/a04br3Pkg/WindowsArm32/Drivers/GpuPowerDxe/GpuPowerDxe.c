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
#include "GpuClockRecord.h"
#include "../../Include/PwrapRead.h"
#include <Library/UefiRuntimeServicesTableLib.h>
#ifndef A04BR3_GPU_MMPLL
#define A04BR3_GPU_MMPLL 0
#endif

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
STATIC EFI_GUID ClockGuid = MT8167_GPU_CLOCK_GUID;
STATIC CHAR16 ClockName[] = MT8167_GPU_CLOCK_NAME;

/* Read-only snapshot: no PLL or regulator writes. WACS2 has a bounded wait. */
STATIC BOOLEAN ReadClock(MT8167_GPU_CLOCK_RECORD *Record) {
  UINT16 Rails[4];
  CONST UINT16 Addresses[] = { 0x030a, 0x030e, 0x0312, 0x0314 };
  UINTN Index;
  if ((MmioRead32(WACS_DATA) & BIT21) == 0) return FALSE;
  Record->version = 1;
  Record->bytes = sizeof(*Record);
  Record->parent = 7;
  Record->con0 = MmioRead32(0x10018160U);
  Record->con1 = MmioRead32(0x10018164U);
  Record->power = MmioRead32(0x10018170U);
  for (Index = 0; Index < 4; ++Index) {
    if (EFI_ERROR(ReadPmic(Addresses[Index], &Rails[Index]))) return FALSE;
  }
  Record->rail_control = Rails[0];
  Record->rail_status = Rails[1];
  Record->sel0 = Rails[2];
  Record->sel1 = Rails[3];
  Record->clock_hz = a04_rgx_mmpll_hz(Record->con0, Record->con1, Record->power,
    Rails[0], Rails[1], Rails[2], Rails[3]);
  return Record->clock_hz != 0 &&
    Record->con0 == MmioRead32(0x10018160U) &&
    Record->con1 == MmioRead32(0x10018164U) &&
    Record->power == MmioRead32(0x10018170U);
}
STATIC BOOLEAN Parent7Selected(VOID) {
  return ((MmioRead32(0x10000000U) >> 8) & 7) == 7 &&
    (MmioRead32(0x10000040U) & ((3U << 18) | (3U << 20))) == 0 &&
    ((MmioRead32(0x10000004U) >> 8) & 63) == 8;
}
STATIC int PowerAndPublish(CONST struct a04_rgx_io *Io) {
  MT8167_GPU_CLOCK_RECORD Before, After;
  EFI_STATUS Status;
  int Result;
  if (A04BR3_GPU_MMPLL && ReadClock(&Before)) {
    Result = a04_rgx_power_on_parent7(Io);
    if (Result == 0 && ReadClock(&After) && Parent7Selected() &&
        Before.con0 == After.con0 && Before.con1 == After.con1 &&
        Before.power == After.power && Before.rail_control == After.rail_control &&
        Before.rail_status == After.rail_status && Before.sel0 == After.sel0 &&
        Before.sel1 == After.sel1) {
      Status = gRT->SetVariable(ClockName, &ClockGuid,
        EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
        sizeof(After), &After);
      DEBUG((DEBUG_ERROR, "A04GPU clock parent7 hz=%u publish=%r\n", After.clock_hz, Status));
      if (!EFI_ERROR(Status)) return 0;
    }
    DEBUG((DEBUG_ERROR, "A04GPU parent7 rejected; restoring parent4\n"));
  }
  /* No record is created before the final successful SetVariable above. */
  return a04_rgx_power_on(Io);
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
  Result = PowerAndPublish(&Io);
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
  EFI_STATUS Status;
  (VOID)ImageHandle;
  (VOID)SystemTable;
  /* Also remove a prior value in OFF builds: absence means parent4/26 MHz. */
  Status = gRT->SetVariable(ClockName, &ClockGuid, 0, 0, NULL);
  if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) return Status;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, GpuBeforeExit, NULL,
                            &gEfiEventBeforeExitBootServicesGuid, &Event);
}
