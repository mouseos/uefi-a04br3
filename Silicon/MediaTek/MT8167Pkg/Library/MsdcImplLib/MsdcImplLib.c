/**
  MT8167 MSDC platform hooks for MediaTekPkg/Drivers/MsdcDxe.

  Host capabilities follow the Linux mtk-sd "mediatek,mt8167-mmc" data
  (a04br3-linux-drivers/msdc/mtk-sd.c mt8167_compat): PAD_TUNE0 at 0xF0, no
  async FIFO, busy check, stop clock fix or enhanced RX, no MSDC top block.
  The A04BR3 DT gives no "mediatek,tuning-step", so the Linux default
  PAD_DELAY_HALF (32) is used.

  Clock control repeats the register writes of the working edk2-mtk A04BR3
  SdCardBlockIoDxe/EmmcBlockIoDxe:
    TOPCKGEN 0x10000000  mux: MSDC0 bits [13:11], MSDC1 bits [22:20], 2 = 188 MHz
    TOPCKGEN 0x10000054/0x10000084  gate set/clear: MSDC0 BIT17, MSDC1 BIT18
    INFRA    0x1000009C/0x1000006C  gate set/clear: MSDC0 BIT28, MSDC1 BIT29

  Power rails and pin muxing are left as LK configured them; the MT6392
  regulators and the MT8167 pinmux layout are not ported (see GpioImplLib).

  The A04BR3 firmware uses the ported EmmcBlockIoDxe/SdCardBlockIoDxe, so
  MsdcDxe is not placed in its firmware volume.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/MsdcImplLib.h>

#define MT8167_TOPCKGEN_CLK_MUX       0x10000000U
#define MT8167_TOPCKGEN_CG_SET        0x10000054U
#define MT8167_TOPCKGEN_CG_CLR        0x10000084U
#define MT8167_INFRA_CG_SET           0x1000009CU
#define MT8167_INFRA_CG_CLR           0x1000006CU
#define MT8167_MSDC_SOURCE_CLOCK_HZ   188000000U
#define MT8167_MSDC_MUX_SEL_188MHZ    0x2U

STATIC CONST UINT32 mMuxShift[2]   = { 11, 20 };
STATIC CONST UINT32 mTopGate[2]    = { BIT17, BIT18 };
STATIC CONST UINT32 mInfraGate[2]  = { BIT28, BIT29 };

MSDC_PLATFORM_INFO gPlatformInfo = {
  .NumberOfHosts  = 2,
  .UseTop         = FALSE,
  .MsdcPadTuneReg = 0xF0,
  .TuningStep     = { 32, 32 },
  .AsyncFifo      = FALSE,
  .BusyCheck      = FALSE,
  .StopClkFix     = FALSE,
  .EnhanceRx      = FALSE
};

VOID
GetSourceClockRate (
  UINT32 Index,
  UINTN *Hz)
{
  *Hz = MT8167_MSDC_SOURCE_CLOCK_HZ;
}

VOID
SourceClockControl (
  UINT32 Index,
  BOOLEAN Enable)
{
  UINT32 Value;

  if (Index > 1) {
    return;
  }

  // Close the gate; for enable, select the 188 MHz parent, then open it.
  MmioWrite32 (MT8167_TOPCKGEN_CG_SET, mTopGate[Index]);
  if (!Enable) {
    return;
  }

  Value  = MmioRead32 (MT8167_TOPCKGEN_CLK_MUX);
  Value &= ~(0x7U << mMuxShift[Index]);
  Value |= MT8167_MSDC_MUX_SEL_188MHZ << mMuxShift[Index];
  MmioWrite32 (MT8167_TOPCKGEN_CLK_MUX, Value);

  MmioWrite32 (MT8167_TOPCKGEN_CG_CLR, mTopGate[Index]);
}

VOID
ClockControl (
  UINT32 Index,
  BOOLEAN Enable)
{
  if (Index > 1) {
    return;
  }

  MmioWrite32 (Enable ? MT8167_INFRA_CG_CLR : MT8167_INFRA_CG_SET, mInfraGate[Index]);
}

VOID
PowerControl (
  UINT32 Index,
  BOOLEAN Enable)
{
  // VEMC/VMCH/VMC stay as LK left them.
}

VOID
InitGpio (
  UINT32 Index)
{
  // MSDC pins stay in the function LK selected.
}
