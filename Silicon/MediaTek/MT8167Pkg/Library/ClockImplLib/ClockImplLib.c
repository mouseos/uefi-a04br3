/**
  MT8167 clock table for MediaTekPkg/Drivers/ClockDxe.

  LK leaves the MT8167 PLLs, muxes and gates that the A04BR3 firmware needs
  running, and the ported A04BR3 drivers (display, eMMC/SD) program their own
  gates exactly as the working edk2-mtk drivers do. ClockDxe is therefore not
  placed in the A04BR3 firmware volume, and this table only lists the fixed
  rates the A04BR3 device tree describes:

    clk26m                26 MHz  (fixed-clock "clk26m", UART0 baud clock)
    clk32k                32 kHz  (fixed-clock "clk32k")
    MSDC0/MSDC1 source   188 MHz  (msdc1-source-clock; edk2-mtk
                                   SdCardBlockIoDxe MSDC1_SOURCE_CLOCK_HZ,
                                   used for both MSDC hosts)

  The MT8167 PLL/mux/gate tree is not described here.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/ClockImplLib.h>

typedef enum {
  TOP_CLK26M,
  TOP_CLK32K,
  TOP_MSDC0_SRC,
  TOP_MSDC1_SRC,
  MAX_CLOCK_ID
} MT8167_CLOCK_ID;

MTK_CLOCK_DESC gClocks[MAX_CLOCK_ID] = {
  {
    .Id    = TOP_CLK26M,
    .Name  = "TOP_CLK26M",
    .Type  = ClockTypeFixed,
    .Fixed = 26000000,
  },
  {
    .Id    = TOP_CLK32K,
    .Name  = "TOP_CLK32K",
    .Type  = ClockTypeFixed,
    .Fixed = 32000,
  },
  {
    .Id    = TOP_MSDC0_SRC,
    .Name  = "TOP_MSDC0_SRC",
    .Type  = ClockTypeFixed,
    .Fixed = 188000000,
  },
  {
    .Id    = TOP_MSDC1_SRC,
    .Name  = "TOP_MSDC1_SRC",
    .Type  = ClockTypeFixed,
    .Fixed = 188000000,
  },
};

UINTN gClockCount = ARRAY_SIZE (gClocks);
