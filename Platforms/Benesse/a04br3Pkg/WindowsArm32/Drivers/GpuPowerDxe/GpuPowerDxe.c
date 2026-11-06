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

/* Diagnostics are emitted once at the end, never from bounded polling loops.
 * RailMask distinguishes an unread/failed rail from an actual raw zero. */
typedef struct {
  MT8167_GPU_CLOCK_RECORD Record;
  UINT32 Wacs;
  UINT32 RailMask;
  UINT32 FailedAddress;
  EFI_STATUS ReadStatus;
  UINT32 VerifyCon0, VerifyCon1, VerifyPower;
} CLOCK_SNAPSHOT;

/* Read-only snapshot: no PLL or regulator writes. WACS2 has a bounded wait. */
STATIC CONST CHAR8 *ReadClock(CLOCK_SNAPSHOT *Snapshot) {
  MT8167_GPU_CLOCK_RECORD *Record = &Snapshot->Record;
  UINT16 Rail;
  CONST UINT16 Addresses[] = { 0x030a, 0x030e, 0x0312, 0x0314 };
  UINT32 *Fields[] = { &Record->rail_control, &Record->rail_status,
                       &Record->sel0, &Record->sel1 };
  UINTN Index;
  Record->version = 1;
  Record->bytes = sizeof(*Record);
  Record->parent = 7;
  Record->con0 = MmioRead32(0x10018160U);
  Record->con1 = MmioRead32(0x10018164U);
  Record->power = MmioRead32(0x10018170U);
  Snapshot->Wacs = MmioRead32(WACS_DATA);
  if ((Snapshot->Wacs & BIT21) == 0) return "pwrap-not-ready";
  for (Index = 0; Index < 4; ++Index) {
    Snapshot->ReadStatus = ReadPmic(Addresses[Index], &Rail);
    if (EFI_ERROR(Snapshot->ReadStatus)) {
      Snapshot->FailedAddress = Addresses[Index];
      return "pwrap-read";
    }
    *Fields[Index] = Rail;
    Snapshot->RailMask |= 1U << Index;
  }
  Record->clock_hz = a04_rgx_mmpll_hz(Record->con0, Record->con1, Record->power,
    (UINT16)Record->rail_control, (UINT16)Record->rail_status,
    (UINT16)Record->sel0, (UINT16)Record->sel1);
  /* Explain rejection without changing the shared acceptance policy. */
  if ((Record->con0 & 1) == 0) return "pll-disabled";
  if ((Record->power & 3) != 1) return "pll-power";
  if ((Record->rail_status & 0x2001) != 0x2001) return "rail-status";
  if (Record->clock_hz == 0) {
    UINT32 Selector = ((Record->rail_control & 2) ? Record->sel1 : Record->sel0) & 0x7f;
    UINT32 Uv = 700000 + 6250 * Selector;
    return (Uv < 1150000 || Uv > 1300000) ? "rail-voltage" : "pll-rate";
  }
  Snapshot->VerifyCon0 = MmioRead32(0x10018160U);
  Snapshot->VerifyCon1 = MmioRead32(0x10018164U);
  Snapshot->VerifyPower = MmioRead32(0x10018170U);
  if (Record->con0 != Snapshot->VerifyCon0 || Record->con1 != Snapshot->VerifyCon1 ||
      Record->power != Snapshot->VerifyPower) return "pll-unstable";
  return NULL;
}
STATIC VOID LogSnapshot(CONST CHAR8 *Stage, CONST CLOCK_SNAPSHOT *Snapshot) {
  CONST MT8167_GPU_CLOCK_RECORD *R = &Snapshot->Record;
  DEBUG((DEBUG_ERROR, "A04GPU clock %a con0=%08x con1=%08x power=%08x rail_control=%08x rail_status=%08x sel0=%08x sel1=%08x hz=%u wacs=%08x rail_mask=%x failed_addr=%04x read=%r verify=%08x/%08x/%08x\n",
    Stage, R->con0, R->con1, R->power, R->rail_control, R->rail_status,
    R->sel0, R->sel1, R->clock_hz, Snapshot->Wacs, Snapshot->RailMask,
    Snapshot->FailedAddress, Snapshot->ReadStatus,
    Snapshot->VerifyCon0, Snapshot->VerifyCon1, Snapshot->VerifyPower));
}
STATIC BOOLEAN Parent7Selected(VOID) {
  return ((MmioRead32(0x10000000U) >> 8) & 7) == 7 &&
    (MmioRead32(0x10000040U) & ((3U << 18) | (3U << 20))) == 0 &&
    ((MmioRead32(0x10000004U) >> 8) & 63) == 8;
}
STATIC int PowerAndPublish(CONST struct a04_rgx_io *Io) {
  CLOCK_SNAPSHOT Before = {0}, After = {0};
  CONST CHAR8 *Reason = "flag-off";
  CONST CHAR8 *Stage = "off";
  EFI_STATUS PublishStatus = EFI_NOT_READY;
  int Parent7Result = -1, Result;
  BOOLEAN AfterRead = FALSE;
  if (A04BR3_GPU_MMPLL) {
    Stage = "before";
    Reason = ReadClock(&Before);
    if (Reason == NULL) {
      Stage = "power-on";
      Parent7Result = a04_rgx_power_on_parent7(Io);
      Reason = "power-on";
      if (Parent7Result == 0) {
        Stage = "after";
        AfterRead = TRUE;
        Reason = ReadClock(&After);
        if (Reason == NULL && !Parent7Selected()) Reason = "clock-route";
        if (Reason == NULL &&
            (Before.Record.con0 != After.Record.con0 || Before.Record.con1 != After.Record.con1 ||
             Before.Record.power != After.Record.power || Before.Record.rail_control != After.Record.rail_control ||
             Before.Record.rail_status != After.Record.rail_status || Before.Record.sel0 != After.Record.sel0 ||
             Before.Record.sel1 != After.Record.sel1)) Reason = "snapshot-changed";
        if (Reason == NULL) {
          Stage = "publish";
          PublishStatus = gRT->SetVariable(ClockName, &ClockGuid,
            EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
            sizeof(After.Record), &After.Record);
          if (EFI_ERROR(PublishStatus)) Reason = "publish";
        }
      }
    }
  }
  /* No record is created before the final successful SetVariable above. */
  Result = (Reason == NULL) ? 0 : a04_rgx_power_on(Io);
  if (A04BR3_GPU_MMPLL) LogSnapshot("before", &Before);
  if (AfterRead) LogSnapshot("after", &After);
  DEBUG((DEBUG_ERROR, "A04GPU clock outcome=%a stage=%a reason=%a parent7_result=%d publish=%r fallback_result=%d\n",
    Reason == NULL ? "parent7" : "parent4-requested", Stage,
    Reason == NULL ? "accepted" : Reason, Parent7Result, PublishStatus,
    Reason == NULL ? -1 : Result));
  return Result;
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
