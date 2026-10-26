/**
  Copyright (c) 2011-2012, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/IoLib.h>
#include <Library/PlatformSecBoardLib.h>
#include <Library/PlatformSecLib.h>

#include "PlatformRegisters.h"

VOID
DisableWatchDogTimer ()
{
  // Fixed MT8167 TOPRGU MODE, as the existing A04BR3AutoReset.cpp and
  // A04BR3TestCycleLib. Do not consult MemoryMapLib here: it can ASSERT
  // before the immediately following board hook has armed the guard.
  MmioWrite32 (WDT_MODE_REGISTER, WDT_MODE_KEY);
}

VOID
PlatformInitialize ()
{
  // LK has already set up clocks, the PMIC wrapper, UART and display
  // (edk2-mtk MT8167 PlatformPrePiLib does nothing else either).
  // The watchdog LK left running is stopped here; a board that wants it
  // arms it again from its PlatformSecBoardLib instance.
  DisableWatchDogTimer ();
  PlatformSecBoardInitialize ();
}
