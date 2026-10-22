/**
  MT8167 GPIO register layout for MediaTekPkg/Drivers/GpioDxe.

  Offsets from Linux drivers/pinctrl/mediatek/pinctrl-mt8167.c
  (dir 0x000, dout 0x100, din 0x200, pinmux 0x300, port_align 4: SET at +0x4,
  CLR at +0x8), which also match edk2-mtk MT8167Pkg/Library/MtkGpioImplLib.
  The last pin is GPIO124 (pinctrl-mtk-mt8167.h).

  MT8167 packs 16 pins per DIR/DOUT/DIN register (port_shf 4) and 5 pins with
  3-bit fields per MODE register, while GpioDxe assumes 32 pins per data
  register and 8 pins with 4-bit fields per mode register. GpioDxe is
  therefore not placed in the A04BR3 firmware volume; this library exists so
  the components listed in MediaTekPkg.dsc.inc build.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/GpioImplLib.h>

MTK_GPIO_PLATFORM_INFO gPlatformInfo = {
  .SetOffset     = 0x4,
  .ResetOffset   = 0x8,
  .DirOffset     = 0x0,
  .DataOutOffset = 0x100,
  .DataInOffset  = 0x200,
  .ModeOffset    = 0x300,
  .MaxPin        = 124,
};
