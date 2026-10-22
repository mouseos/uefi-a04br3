/**
  Copyright (c) 2011-2012, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PlatformSecLib.h>

#include "PlatformRegisters.h"

VOID
DisableWatchDogTimer ()
{
  EFI_STATUS                   Status;
  EFI_MEMORY_REGION_DESCRIPTOR WDogRegion;

  // Locate "WatchDog Timer" Memory Region
  Status = LocateMemoryRegionByName ("WatchDog Timer", &WDogRegion);
  if (EFI_ERROR (Status)) {
    return;
  }

  // Disable WatchDog Timer
  MmioWrite32 (WDogRegion.Address, WDT_MODE_KEY);
}

VOID
PlatformInitialize ()
{
  // LK has already set up clocks, the PMIC wrapper, UART and display
  // (edk2-mtk MT8167 PlatformPrePiLib does nothing else either).
  // The watchdog LK left running is stopped here; a board that wants it
  // (a04br3Pkg A04BR3TestCycleDxe with A04BR3_TEST_CYCLE=1) arms it again
  // early in DXE, where a timer event can kick it.
  DisableWatchDogTimer ();
}
