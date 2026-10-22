/** @file
  Benesse TAB-A04-BR3 (MediaTek MT8167) unattended test cycle, firmware side.

  Goal: whatever the firmware or the kernel does after this driver starts,
  the tablet ends up in the stock LK fastboot mode, where the host boots or
  flashes the next image. This needs two things:

   1. Bit 13 of the PMIC RTC register RTC_PDN1 (0x802C). The stock LK
      (CT3-01.06.000) checks that bit first and, when set, prints
      "[FASTBOOT] reboot to boot loader", enters fastboot mode and clears it
      (REA of the stock LK: FUN_41e032e4, FUN_41e0667c, FUN_41e06638; see
      unixnu/_research/lk/lk-fastboot-entry.json). With the bit set, every
      reset lands in fastboot.
   2. A reset when progress stops. SEC (MT8167Pkg PlatformSecLib) stops the
      MT8167 TOPRGU hardware watchdog that LK left running and then calls the
      board hook (PlatformSecBoardLib); with the test cycle the a04br3 hook
      (Library/A04BR3PlatformSecBoardLib) sets the flag and arms the watchdog
      for 31 s at once, so a hang in SEC, PrePi or early DXE also resets into
      fastboot. This driver keeps the watchdog alive from a periodic timer
      event.

  Timeline:
   - SEC: flag set, watchdog armed for 31 s (not kicked).
   - Driver entry (dispatched from the APRIORI list, right after the timer
     and PMIC wrapper drivers, see Include/APRIORI.inc): set the fastboot
     flag, arm the watchdog, start kicking every second. Each kick arms the
     watchdog again if another driver disabled it and retries the flag if it
     was not set. A hang anywhere in
     DXE or BDS that blocks timer callbacks (TPL_CALLBACK) stops the kicks and
     the watchdog resets into fastboot.
   - No ExitBootServices within FIRMWARE_STAGE_SECONDS (120 s) after entry,
     for example because the payload is missing and the firmware waits in the
     boot menu or the shell: the kicks stop, so the watchdog resets into
     fastboot at most 31 s later.
   - ExitBootServices: the kicks stop, the flag is set again and the watchdog
     is re-armed for a full 31 s. The kernel side, unixnu_board_init() in
     UniXNU-a04br3 package/A04BR3AutoReset.cpp (boot argument
     a04br3-autoreset=<seconds>, called from bsd_init), repeats step 1, re-arms
     the watchdog and kicks it from a kernel thread. A kernel that stops before
     bsd_init is reset into fastboot.

  The driver is only part of the firmware with -D A04BR3_TEST_CYCLE=1 (default
  0); the default firmware is unchanged and never touches the flag or the
  watchdog after SEC.

  Register access: Library/A04BR3TestCycleLib.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/A04BR3TestCycleLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Guid/EventGroup.h>

#define WATCHDOG_SECONDS        A04BR3_WATCHDOG_SECONDS

// Kick period, in 100 ns units: 1 s, well inside the 31 s timeout.
#define KICK_PERIOD             10000000ULL

// Longest time the firmware may stay before ExitBootServices while the
// watchdog is kept alive. A normal UniXNU boot reaches ExitBootServices in a
// few seconds (DarwinBoot loads the kernel from the RAM payload volume).
#define FIRMWARE_STAGE_SECONDS  120

STATIC EFI_EVENT  mKickEvent;
STATIC EFI_EVENT  mExitBootServicesEvent;
STATIC UINTN      mKicks;
STATIC BOOLEAN    mKicking;
STATIC BOOLEAN    mFlagSet;

STATIC
VOID
StopKicking (
  VOID
  )
{
  mKicking = FALSE;
  gBS->SetTimer (mKickEvent, TimerCancel, 0);
}

STATIC
VOID
EFIAPI
OnKickTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (!mKicking) {
    return;
  }

  // One callback per KICK_PERIOD (1 s); late callbacks only make this longer.
  if (++mKicks >= FIRMWARE_STAGE_SECONDS) {
    StopKicking ();
    DEBUG ((
      DEBUG_ERROR,
      "A04BR3TestCycleDxe: no ExitBootServices within %u s; watchdog kicks stopped, reset into fastboot within %u s\n",
      FIRMWARE_STAGE_SECONDS,
      WATCHDOG_SECONDS
      ));
    return;
  }

  // A driver that writes TOPRGU MODE (for example to disable the watchdog)
  // must not end the test cycle: arm it again.
  if (!A04BR3WatchdogEnabled ()) {
    DEBUG ((DEBUG_ERROR, "A04BR3TestCycleDxe: watchdog found disabled, armed again\n"));
    A04BR3WatchdogArm (WATCHDOG_SECONDS);
  }

  // Without the flag a watchdog reset would boot the same image again.
  if (!mFlagSet) {
    mFlagSet = A04BR3SetFastbootFlag ();
    DEBUG ((DEBUG_ERROR, "A04BR3TestCycleDxe: fastboot flag retry: %a\n", mFlagSet ? "set" : "NOT set"));
  }

  A04BR3WatchdogKick ();
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

  // The kernel takes the watchdog over in bsd_init (A04BR3AutoReset.cpp).
  StopKicking ();
  Flag = A04BR3SetFastbootFlag ();
  A04BR3WatchdogArm (WATCHDOG_SECONDS);
  DEBUG ((
    DEBUG_ERROR,
    "A04BR3TestCycleDxe: ExitBootServices: fastboot flag %a, watchdog re-armed for %u s\n",
    Flag ? "set" : "NOT set",
    WATCHDOG_SECONDS
    ));
}

EFI_STATUS
EFIAPI
A04BR3TestCycleDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  BOOLEAN     Flag;

  // Fastboot first: from here on any reset, including the watchdog's, must
  // return to LK fastboot.
  Flag     = A04BR3SetFastbootFlag ();
  mFlagSet = Flag;
  A04BR3WatchdogArm (WATCHDOG_SECONDS);

  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnKickTimer, NULL, &mKickEvent);
  if (!EFI_ERROR (Status)) {
    mKicking = TRUE;
    Status   = gBS->SetTimer (mKickEvent, TimerPeriodic, KICK_PERIOD);
  }

  if (EFI_ERROR (Status)) {
    // Without kicks the armed watchdog resets into fastboot within 31 s.
    DEBUG ((DEBUG_ERROR, "A04BR3TestCycleDxe: kick timer %r; the watchdog resets in %u s\n", Status, WATCHDOG_SECONDS));
    return EFI_SUCCESS;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OnExitBootServices,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &mExitBootServicesEvent
                  );
  ASSERT_EFI_ERROR (Status);

  DEBUG ((
    DEBUG_ERROR,
    "A04BR3TestCycleDxe: fastboot flag %a, watchdog %u s armed and kicked until ExitBootServices (at most %u s)\n",
    Flag ? "set" : "NOT set",
    WATCHDOG_SECONDS,
    FIRMWARE_STAGE_SECONDS
    ));
  return EFI_SUCCESS;
}
