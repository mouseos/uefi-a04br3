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
      MT8167 TOPRGU hardware watchdog that LK left running; this driver arms
      it again with its longest timeout (31 s) and kicks it from a periodic
      timer event.

  Timeline:
   - Driver entry (dispatched from the APRIORI list, right after the timer
     and PMIC wrapper drivers, see Include/APRIORI.inc): set the fastboot
     flag, arm the watchdog, start kicking every second. A hang anywhere in
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

  Register sequences (identical to A04BR3AutoReset.cpp):
   - PWRAP WACS2 (0x1000F000 +0xA0 CMD, +0xA4 RDATA with FSM bits [18:16] and
     INIT_DONE bit 21, +0xA8 VLDCLR) and the RTC unlock / write trigger order
     are those of the stock LK (FUN_41e0588c, FUN_41e06280, FUN_41e06254).
     LK has initialized the wrapper before it starts the kernel slot. The
     PmicWrapperDxe protocol is not used: it neither checks INIT_DONE nor
     clears a pending valid while waiting for idle, and waits without bound.
   - TOPRGU 0x10007000 (MODE key 0x22000000, LENGTH key 0x08, RESTART key
     0x1971) follows Linux drivers/watchdog/mtk_wdt.c (mtk_wdt_set_timeout,
     mtk_wdt_start, mtk_wdt_ping).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Guid/EventGroup.h>

#define PWRAP_BASE              0x1000F000U
#define PWRAP_WACS2_CMD         (PWRAP_BASE + 0xA0)
#define PWRAP_WACS2_RDATA       (PWRAP_BASE + 0xA4)
#define PWRAP_WACS2_VLDCLR      (PWRAP_BASE + 0xA8)
#define PWRAP_INIT_DONE         BIT21
#define PWRAP_FSM_IDLE          0
#define PWRAP_FSM_WFVLDCLR      6

#define RTC_BBPU                0x8000
#define RTC_BBPU_CBUSY          BIT6
#define RTC_PDN1                0x802C
#define RTC_PDN1_FASTBOOT       BIT13
#define RTC_PROT                0x8036
#define RTC_WRTGR               0x803C

#define TOPRGU_BASE             0x10007000U
#define TOPRGU_MODE             (TOPRGU_BASE + 0x00)
#define TOPRGU_LENGTH           (TOPRGU_BASE + 0x04)
#define TOPRGU_RESTART          (TOPRGU_BASE + 0x08)
#define TOPRGU_RESTART_KEY      0x1971

// TOPRGU LENGTH holds at most 31 s (Linux mtk_wdt max_timeout).
#define WATCHDOG_SECONDS        31

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

STATIC
VOID
Write32 (
  IN UINTN   Address,
  IN UINT32  Value
  )
{
  MmioWrite32 (Address, Value);
  ArmDataSynchronizationBarrier ();  // dsb sy, as A04BR3AutoReset.cpp
}

/**
  One WACS2 transaction, as the stock LK: wait for the idle FSM (clearing a
  pending valid), issue, and for a read wait for valid data. Bounded: FALSE on
  timeout or when the wrapper never finished its initialization.
**/
STATIC
BOOLEAN
Wacs (
  IN  BOOLEAN  IsWrite,
  IN  UINT32   Address,
  IN  UINT32   Data,
  OUT UINT32   *Out OPTIONAL
  )
{
  UINT32  Status;
  UINT32  Fsm;
  UINTN   Spin;

  for (Spin = 0; ; Spin++) {
    Status = MmioRead32 (PWRAP_WACS2_RDATA);
    if (((Status & PWRAP_INIT_DONE) == 0) || (Spin > 1000000)) {
      return FALSE;
    }

    Fsm = (Status >> 16) & 7;
    if (Fsm == PWRAP_FSM_IDLE) {
      break;
    }

    if (Fsm == PWRAP_FSM_WFVLDCLR) {
      Write32 (PWRAP_WACS2_VLDCLR, 1);
    }
  }

  Write32 (PWRAP_WACS2_CMD, (IsWrite ? BIT31 : 0) | ((Address >> 1) << 16) | (Data & 0xFFFF));
  if (IsWrite) {
    return TRUE;
  }

  for (Spin = 0; ; Spin++) {
    Status = MmioRead32 (PWRAP_WACS2_RDATA);
    if (Spin > 1000000) {
      return FALSE;
    }

    if (((Status >> 16) & 7) == PWRAP_FSM_WFVLDCLR) {
      *Out = Status & 0xFFFF;
      Write32 (PWRAP_WACS2_VLDCLR, 1);
      return TRUE;
    }
  }
}

STATIC
BOOLEAN
RtcTrigger (
  VOID
  )
{
  UINT32  Bbpu;
  UINTN   Spin;

  if (!Wacs (TRUE, RTC_WRTGR, 1, NULL)) {
    return FALSE;
  }

  for (Spin = 0; Spin < 100000; Spin++) {
    Bbpu = 0;
    if (!Wacs (FALSE, RTC_BBPU, 0, &Bbpu)) {
      return FALSE;
    }

    if ((Bbpu & RTC_BBPU_CBUSY) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
BOOLEAN
SetFastbootFlag (
  VOID
  )
{
  UINT32  Pdn1;
  UINT32  Check;

  Pdn1 = 0;
  if (!Wacs (TRUE, RTC_PROT, 0x586A, NULL) || !RtcTrigger () ||
      !Wacs (TRUE, RTC_PROT, 0x9136, NULL) || !RtcTrigger () ||
      !Wacs (FALSE, RTC_PDN1, 0, &Pdn1) ||
      !Wacs (TRUE, RTC_PDN1, (Pdn1 & 0xFFFF) | RTC_PDN1_FASTBOOT, NULL) || !RtcTrigger ())
  {
    return FALSE;
  }

  Check = 0;
  return Wacs (FALSE, RTC_PDN1, 0, &Check) && ((Check & RTC_PDN1_FASTBOOT) != 0);
}

STATIC
VOID
WatchdogArm (
  IN UINT32  Seconds
  )
{
  UINT32  Mode;

  Write32 (TOPRGU_LENGTH, ((Seconds << 6) << 5) | 0x08);
  Write32 (TOPRGU_RESTART, TOPRGU_RESTART_KEY);
  // Enable; no interrupt, no dual mode (Linux mtk_wdt_start).
  Mode = (MmioRead32 (TOPRGU_MODE) & 0x00FFFFFFU & ~0x08U & ~0x40U) | 0x22000001U;
  Write32 (TOPRGU_MODE, Mode);
}

STATIC
VOID
WatchdogKick (
  VOID
  )
{
  Write32 (TOPRGU_RESTART, TOPRGU_RESTART_KEY);  // Linux mtk_wdt_ping
}

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

  WatchdogKick ();
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
  Flag = SetFastbootFlag ();
  WatchdogArm (WATCHDOG_SECONDS);
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
  Flag = SetFastbootFlag ();
  WatchdogArm (WATCHDOG_SECONDS);

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
