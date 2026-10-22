/** @file
  Benesse TAB-A04-BR3 (MediaTek MT8167) unattended test cycle: register
  access shared by SEC (A04BR3PlatformSecBoardLib) and DXE
  (A04BR3TestCycleDxe). Plain MMIO with bounded polling and no boot services,
  so it runs in every phase.

  Register sequences (identical to UniXNU-a04br3 package/A04BR3AutoReset.cpp):
   - PWRAP WACS2 (0x1000F000 +0xA0 CMD, +0xA4 RDATA with FSM bits [18:16] and
     INIT_DONE bit 21, +0xA8 VLDCLR) and the RTC unlock / write trigger order
     are those of the stock LK (FUN_41e0588c, FUN_41e06280, FUN_41e06254).
     LK has initialized the wrapper before it starts the kernel slot.
   - Bit 13 of PMIC RTC_PDN1 (0x802C) makes the stock LK (CT3-01.06.000)
     print "[FASTBOOT] reboot to boot loader", enter fastboot and clear the
     bit (FUN_41e032e4, FUN_41e0667c, FUN_41e06638; unixnu/_research/lk/
     lk-fastboot-entry.json in the porting workspace).
   - TOPRGU 0x10007000 (MODE key 0x22000000, LENGTH key 0x08, RESTART key
     0x1971) follows Linux drivers/watchdog/mtk_wdt.c (mtk_wdt_set_timeout,
     mtk_wdt_start, mtk_wdt_ping).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>

#include <Library/A04BR3TestCycleLib.h>
#include <Library/ArmLib.h>
#include <Library/IoLib.h>

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

BOOLEAN
EFIAPI
A04BR3SetFastbootFlag (
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

VOID
EFIAPI
A04BR3WatchdogArm (
  IN UINT32  Seconds
  )
{
  UINT32  Mode;

  Write32 (TOPRGU_LENGTH, ((Seconds << 6) << 5) | 0x08);
  Write32 (TOPRGU_RESTART, TOPRGU_RESTART_KEY);
  // Enable with the external reset and auto restart bits that the reset
  // path proven on this tablet sets (A04BR3AutoReset.cpp resetNow: 0x14),
  // no interrupt, no dual mode (Linux mtk_wdt: EXRST_EN BIT(2),
  // IRQ_EN BIT(3), AUTO_START BIT(4), DUAL_EN BIT(6)).
  Mode = (MmioRead32 (TOPRGU_MODE) & 0x00FFFFFFU & ~0x48U) | 0x22000015U;
  Write32 (TOPRGU_MODE, Mode);
}

VOID
EFIAPI
A04BR3WatchdogKick (
  VOID
  )
{
  Write32 (TOPRGU_RESTART, TOPRGU_RESTART_KEY);  // Linux mtk_wdt_ping
}

BOOLEAN
EFIAPI
A04BR3WatchdogEnabled (
  VOID
  )
{
  return (MmioRead32 (TOPRGU_MODE) & BIT0) != 0;
}
