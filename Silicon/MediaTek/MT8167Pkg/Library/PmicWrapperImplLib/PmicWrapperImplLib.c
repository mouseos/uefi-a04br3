/**
  MT8167 PMIC wrapper (PWRAP at 0x1000F000, A04BR3 DT "pwrap@1000f000").

  MT8167 uses the MT8516 wrapper register layout (A04BR3 Linux driver
  a04br3-linux-drivers/pmic/mtk-pmic-wrap.c, pwrap_mt8167 -> mt8516_regs):
  INIT_DONE2 0x9C, WACS2_CMD 0xA0, WACS2_RDATA 0xA4, WACS2_VLDCLR 0xA8.
  The wrapper has no PWRAP_CAP_ARB, so the classic WACS2 command and RDATA
  FSM encoding is used. The same WACS2 offsets are used by the working
  edk2-mtk A04BR3 DisplayInitDxe and CpuPerformanceDxe.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/PmicWrapperImplLib.h>

MTK_PMIC_WRAPPER_PLATFORM_INFO gPlatformInfo = {
  .RegMap = {
    [PmicWrapperInitDone2]   = 0x09C,
    [PmicWrapperWacs2Cmd]    = 0x0A0,
    [PmicWrapperWacs2RData]  = 0x0A4,
    [PmicWrapperWacs2VldClr] = 0x0A8,
  },
  .ArbCapabilities = FALSE,
};
