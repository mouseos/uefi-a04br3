/** @file
  Benesse TAB-A04-BR3 board hook of the SEC phase, only mapped with the
  unattended test cycle (-D A04BR3_TEST_CYCLE=1, see a04br3.dsc).

  Sets the LK fastboot flag and arms the TOPRGU watchdog for 31 s right after
  SEC stopped the watchdog LK left running. A hang in SEC, PrePi, DxeCore or
  the APRIORI drivers before A04BR3TestCycleDxe takes over (it kicks the
  watchdog from a timer event) then resets into LK fastboot instead of
  booting the same image again.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>

#include <Library/A04BR3TestCycleLib.h>
#include <Library/DebugLib.h>
#include <Library/PlatformSecBoardLib.h>

VOID
EFIAPI
PlatformSecBoardInitialize (
  VOID
  )
{
  BOOLEAN  Flag;

  // Arm first: a PMIC wrapper access that stalls still ends in a reset.
  A04BR3WatchdogArm (A04BR3_WATCHDOG_SECONDS);
  Flag = A04BR3SetFastbootFlag ();
  DEBUG ((
    DEBUG_ERROR,
    "A04BR3 SEC: test cycle: fastboot flag %a, watchdog armed for %u s\n",
    Flag ? "set" : "NOT set",
    A04BR3_WATCHDOG_SECONDS
    ));
}
