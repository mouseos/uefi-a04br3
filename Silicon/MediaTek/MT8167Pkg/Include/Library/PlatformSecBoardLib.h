/** @file
  Board hook of the MT8167 SEC phase (MT8167Pkg PlatformSecLib).

  PlatformSecLib calls PlatformSecBoardInitialize() once, after the SoC setup
  (the watchdog LK left running is stopped). The default instance
  (Library/PlatformSecBoardLibNull) does nothing; a board package maps its own
  instance in its DSC when it needs early board setup.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef PLATFORM_SEC_BOARD_LIB_H_
#define PLATFORM_SEC_BOARD_LIB_H_

VOID
EFIAPI
PlatformSecBoardInitialize (
  VOID
  );

#endif
