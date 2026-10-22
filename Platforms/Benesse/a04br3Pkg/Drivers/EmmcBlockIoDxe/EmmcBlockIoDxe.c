/** @file
  Read-only TAB-A04-BR3 eMMC Block I/O driver for the MT8167 MSDC0 host.

  Same arrangement as mouseos/edk2-mtk Platform/Benesse/A04BR3Pkg/Drivers/
  EmmcBlockIoDxe (commit 1982d888c765): the MSDC driver source is shared with
  SdCardBlockIoDxe and built for MSDC0 (0x11120000, 8-bit, GPT verified).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#define A04BR3_EMMC_BUILD
#include "../SdCardBlockIoDxe/SdCardBlockIoDxe.c"
