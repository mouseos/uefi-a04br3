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
