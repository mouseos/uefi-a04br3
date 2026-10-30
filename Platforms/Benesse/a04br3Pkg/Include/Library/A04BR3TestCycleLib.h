/** @file
  Benesse TAB-A04-BR3 unattended test cycle: LK fastboot flag and TOPRGU
  watchdog (Library/A04BR3TestCycleLib).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef A04BR3_TEST_CYCLE_LIB_H_
#define A04BR3_TEST_CYCLE_LIB_H_

// TOPRGU LENGTH holds at most 31 s (Linux mtk_wdt max_timeout).
#define A04BR3_WATCHDOG_SECONDS  31

/**
  Set bit 13 of PMIC RTC_PDN1 so the next reset enters LK fastboot.
  @retval TRUE   The bit reads back set.
  @retval FALSE  The PMIC wrapper or the RTC did not respond (bounded wait).
**/
BOOLEAN
EFIAPI
A04BR3SetFastbootFlag (
  VOID
  );

/** Arm the TOPRGU watchdog for Seconds (at most 31) and restart it. **/
VOID
EFIAPI
A04BR3WatchdogArm (
  IN UINT32  Seconds
  );

/** Restart the TOPRGU watchdog counter. **/
VOID
EFIAPI
A04BR3WatchdogKick (
  VOID
  );

/** Stop the TOPRGU watchdog as MT8167Pkg PlatformSec does (MODE <- key, enable clear). **/
VOID
EFIAPI
A04BR3WatchdogDisable (
  VOID
  );

/** TRUE when the TOPRGU watchdog is enabled (MODE bit 0). **/
BOOLEAN
EFIAPI
A04BR3WatchdogEnabled (
  VOID
  );

/**
  Read PMIC RTC_SPAR0 (0x8030) and, unless Clear and Set are both 0, write
  (value & ~Clear) | Set through the RTC unlock and write trigger, then read it
  back. Linux rtc-mt6397.c keeps bits [5:0] and 8 there, the vendor RTC HAL
  bits 6 and 7, and the stock LK tests bit 15; the a04br3 firmware uses only
  bit 12.
  @param[out] Value  The value read (before the write, or after it when written).
  @retval FALSE  The PMIC wrapper or the RTC did not respond, or the readback differs.
**/
BOOLEAN
EFIAPI
A04BR3RtcSpare0Update (
  IN  UINT32  Clear,
  IN  UINT32  Set,
  OUT UINT32  *Value
  );

/**
  One PMIC register read through PWRAP WACS2 (bounded).
  @retval FALSE  The wrapper did not finish or return data.
**/
BOOLEAN
EFIAPI
A04BR3PmicRead (
  IN  UINT32  Address,
  OUT UINT32  *Value
  );

/**
  One PMIC register write through PWRAP WACS2. As the stock LK the command is
  issued after the FSM is idle; the next transaction waits for its completion,
  so callers verify the effect by reading the register back.
**/
BOOLEAN
EFIAPI
A04BR3PmicWrite (
  IN UINT32  Address,
  IN UINT32  Value
  );

#endif
