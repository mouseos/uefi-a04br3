/** @file
  Benesse TAB-A04-BR3 (MediaTek MT8167) unattended test cycle, firmware side.

  When the kernel takes over (ExitBootServices), this driver
   1. sets bit 13 of the PMIC RTC register RTC_PDN1 (0x802C). The stock LK
      (CT3-01.06.000) checks that bit first and, when set, prints
      "[FASTBOOT] reboot to boot loader", enters fastboot mode and clears it
      (REA of the stock LK: FUN_41e032e4, FUN_41e0667c, FUN_41e06638; see
      unixnu/_research/lk/lk-fastboot-entry.json). Every later reset therefore
      lands in fastboot, where the host boots or flashes the next image;
   2. arms the MT8167 TOPRGU hardware watchdog with its longest timeout
      (31 s). A kernel that stops anywhere before it takes the watchdog over
      (early panic, hang before bsd_init) is reset into fastboot.

  The kernel side is unixnu_board_init() in UniXNU-a04br3
  package/A04BR3AutoReset.cpp (boot argument a04br3-autoreset=<seconds>),
  called from bsd_init: it repeats step 1, re-arms the watchdog and kicks it
  from a kernel thread. The kernel must reach it within 31 s of
  ExitBootServices, or the watchdog resets the tablet.

  Nothing happens before ExitBootServices, so the firmware menu and manual
  use are not affected. The driver is only part of the firmware volume with
  -D A04BR3_TEST_CYCLE=1 (default 0).

  Register sequences (identical to A04BR3AutoReset.cpp):
   - PWRAP WACS2 (0x1000F000 +0xA0 CMD, +0xA4 RDATA with FSM bits [18:16] and
     INIT_DONE bit 21, +0xA8 VLDCLR) and the RTC unlock / write trigger order
     are those of the stock LK (FUN_41e0588c, FUN_41e06280, FUN_41e06254).
     The PmicWrapperDxe protocol is not used: it neither checks INIT_DONE nor
     clears a pending valid while waiting for idle, and waits without bound.
   - TOPRGU 0x10007000 (MODE key 0x22000000, LENGTH key 0x08, RESTART key
     0x1971) follows Linux drivers/watchdog/mtk_wdt.c (mtk_wdt_set_timeout,
     mtk_wdt_start).

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

STATIC EFI_EVENT  mExitBootServicesEvent;

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
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  BOOLEAN  Flag;

  Flag = SetFastbootFlag ();
  WatchdogArm (WATCHDOG_SECONDS);
  DEBUG ((
    DEBUG_ERROR,
    "A04BR3TestCycleDxe: fastboot flag %a, watchdog %u s armed\n",
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

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OnExitBootServices,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &mExitBootServicesEvent
                  );
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_ERROR, "A04BR3TestCycleDxe: fastboot return armed for ExitBootServices\n"));
  return Status;
}
