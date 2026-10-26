/** @file
  Bounded firmware feeding of the SEC-armed MT8167 hardware watchdog.
  Adapted from uefi-a04br3-codex A04BR3TestCycleDxe at
  45a04ea93e99e587c5c01f93e2b6b14e443679b3. Register operations are reused
  unchanged from its A04BR3TestCycleLib / the tested A04BR3AutoReset.cpp.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Library/A04BR3TestCycleLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>

#define KICK_PERIOD             10000000ULL
#define FIRMWARE_STAGE_SECONDS  120

STATIC EFI_EVENT  mKickEvent;
STATIC EFI_EVENT  mExitBootServicesEvent;
STATIC UINT64     mStartCounter;
STATIC UINT64     mDeadlineTicks;
STATIC BOOLEAN    mKicking;
STATIC BOOLEAN    mExited;

STATIC
VOID
EFIAPI
OnKickTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_TPL  OldTpl;

  // EBS is TPL_NOTIFY. Serialize the check and MMIO with that callback so
  // a suspended timer callback cannot resume and feed after EBS stopped it.
  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  if (!mKicking || mExited) {
    gBS->RestoreTPL (OldTpl);
    return;
  }

  // A wall-clock limit, not a callback count: delayed callbacks never extend
  // the feeding interval. Unsigned subtraction tolerates one counter wrap.
  if (GetPerformanceCounter () - mStartCounter >= mDeadlineTicks) {
    mKicking = FALSE;
    gBS->RestoreTPL (OldTpl);
    DEBUG ((DEBUG_ERROR, "A04BR3 recovery: 120 s firmware deadline; WDT feeding stopped\n"));
    return;
  }

  if (!A04BR3WatchdogEnabled ()) {
    A04BR3WatchdogArm (A04BR3_WATCHDOG_SECONDS);
  }

  A04BR3WatchdogKick ();
  gBS->RestoreTPL (OldTpl);
}

STATIC
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  BOOLEAN  Flag;

  mKicking = FALSE;
  if (mExited) {
    return;
  }

  mExited = TRUE;
  // No SetTimer/CloseEvent/allocation here. Give the existing XNU AutoReset
  // path in bsd_init one full 31 s interval; no further firmware feeding.
  A04BR3WatchdogArm (A04BR3_WATCHDOG_SECONDS);
  Flag = A04BR3SetFastbootFlag ();
  DEBUG ((DEBUG_ERROR, "A04BR3 recovery: EBS, RTC fastboot %a, WDT armed %u s; feeding stopped\n",
          Flag ? "verified" : "FAILED", A04BR3_WATCHDOG_SECONDS));
}

EFI_STATUS
EFIAPI
A04BR3TestCycleDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT64      Frequency;
  UINT64      First;
  UINT64      Last;

  A04BR3WatchdogArm (A04BR3_WATCHDOG_SECONDS);
  if (!A04BR3SetFastbootFlag ()) {
    DEBUG ((DEBUG_ERROR, "A04BR3 recovery: DXE RTC unverified; smoke refused, normal boot continues without feeding\n"));
    return EFI_SUCCESS;
  }

  Frequency = GetPerformanceCounterProperties (&First, &Last);
  if ((Frequency == 0) || (First != 0) || (Last != MAX_UINT64) ||
      (Frequency > MAX_UINT64 / FIRMWARE_STAGE_SECONDS))
  {
    DEBUG ((DEBUG_ERROR, "A04BR3 recovery: unsupported counter; no feeding\n"));
    return EFI_SUCCESS;
  }

  mStartCounter = GetPerformanceCounter ();
  mDeadlineTicks = Frequency * FIRMWARE_STAGE_SECONDS;
  mKicking = FALSE;
  mExited = FALSE;
  // Install EBS handling before any timer can feed. Failure never starts it.
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY, OnExitBootServices,
                             NULL, &gEfiEventExitBootServicesGuid, &mExitBootServicesEvent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "A04BR3 recovery: EBS event %r; no feeding\n", Status));
    return EFI_SUCCESS;
  }

  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                           OnKickTimer, NULL, &mKickEvent);
  if (!EFI_ERROR (Status)) {
    mKicking = TRUE;
    Status = gBS->SetTimer (mKickEvent, TimerPeriodic, KICK_PERIOD);
  }

  if (EFI_ERROR (Status)) {
    mKicking = FALSE;
    DEBUG ((DEBUG_ERROR, "A04BR3 recovery: kick timer %r; no feeding\n", Status));
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, "A04BR3 recovery: RTC verified; WDT %u s, feeding deadline %u s\n",
          A04BR3_WATCHDOG_SECONDS, FIRMWARE_STAGE_SECONDS));
#if A04BR3_EARLY_FAULT == 2
  // IRQs may continue and execute the periodic callback: its absolute deadline
  // must still stop feeding and let hardware reset. No ASSERT dependency.
  DEBUG ((DEBUG_ERROR, "A04BR3 RECOVERY SMOKE: DXE deadloop with timer live\n"));
  CpuDeadLoop ();
#endif
  return EFI_SUCCESS;
}
