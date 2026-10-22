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

/** TRUE when the TOPRGU watchdog is enabled (MODE bit 0). **/
BOOLEAN
EFIAPI
A04BR3WatchdogEnabled (
  VOID
  );

#endif
