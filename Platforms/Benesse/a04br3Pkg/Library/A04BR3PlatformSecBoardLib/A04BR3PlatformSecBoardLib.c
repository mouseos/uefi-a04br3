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
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#if A04BR3_CPU_STAGE_DIAG == 1 && !defined(MDEPKG_NDEBUG)
#include <Library/IoLib.h>
#include <A04BR3CpuStage.h>
#endif

#include <Library/PlatformSecBoardLib.h>
#if A04BR3_CPU_1300 == 1
#include <Library/ArmLib.h>
#include <Library/IoLib.h>
#endif

#if A04BR3_CPU_1300 == 1
//
// Optional CPU 1300 MHz (A04BR3_CPU_1300=1, off by default), once in SEC after
// the recovery watchdog and the fastboot flag are in place. Values of the stock
// CT3 kernel-4.4 of this tablet:
//  - OPP (arch/arm64/boot/dts/mediatek/mt8167.dtsi cluster0_opp, a04br3.dts
//    proc-supply = mt6392_vproc_reg): 1300 MHz at 1.30 V, the stock maximum.
//  - VPROC (drivers/regulator/mt6392-regulator.c buck_vproc and
//    drivers/misc/mediatek/include/mt-plat/mt8167/include/mach/upmu_hw.h):
//    VOSEL 0x021E and VOSEL_ON 0x0220 bits [6:0], 0.7 V + 6.25 mV per code
//    (mtk_spm_internal.c VOLT_TO_PMIC_VAL); VOSEL_CTRL 0x0216 bit 1 selects
//    VOSEL_ON; EN 0x021A bit 0; ramp 12.5 mV/us (mt6392.dtsi).
//  - Order of drivers/cpufreq/cpufreq-dt.c (CONFIG_CPUFREQ_DT): voltage up,
//    then the clock. ARMPLL (drivers/clk/mediatek/clk-mt8167.c, clk-pll.c):
//    CON1 0x10018104 PCW [20:0] = MHz / 26 * 2^14, POSTDIV [26:24], then the
//    same register with PCW_CHG bit 31, then 20 us.
// Only the known LK state (CPU mux on ARMPLL, 1001 MHz, VPROC within the stock
// 1.15 V..1.30 V) is changed; anything else is reported and left untouched.
//
#define A04BR3_INFRA_MUX         0x10001000U
#define A04BR3_ARMPLL_CON0       0x10018100U
#define A04BR3_ARMPLL_CON1       0x10018104U
#define A04BR3_ARMPLL_PCW_MASK   0x001FFFFFU
#define A04BR3_ARMPLL_PD_MASK    (0x7U << 24)
#define A04BR3_ARMPLL_PCW_CHG    BIT31
#define A04BR3_ARMPLL_PCW_1001   0x0009A000U  // 38.5 * 2^14: 26 MHz * 38.5 = 1001 MHz
#define A04BR3_ARMPLL_PCW_1300   0x000C8000U  // 50 * 2^14:   26 MHz * 50   = 1300 MHz
#define A04BR3_VPROC_CTRL        0x0216
#define A04BR3_VPROC_EN          0x021A
#define A04BR3_VPROC_VOSEL       0x021E
#define A04BR3_VPROC_VOSEL_ON    0x0220
#define A04BR3_VPROC_CODE_MASK   0x7FU
#define A04BR3_VPROC_CODE_1150   0x48U        // (1150 - 700) / 6.25 = 72
#define A04BR3_VPROC_CODE_1300   0x60U        // (1300 - 700) / 6.25 = 96
// FHCTL (CT3 mach/mtk_fhreg.h REG_FHCTL_HP_EN = APMIXED + 0x14; mtk_freqhopping.c
// pll_id_to_fhctl_id(FH_ARMCA7_PLLID 0) = 0): bit 0 set means hopping owns ARMPLL.
#define A04BR3_FHCTL_HP_EN       0x10018014U
#define A04BR3_DELAY_MAX_POLLS   10000000U

STATIC
UINT32
A04BR3VprocMv (
  IN UINT32  Code
  )
{
  return 700 + (Code * 625) / 100;
}

STATIC
BOOLEAN
A04BR3DelayUs (
  IN UINT32  Microseconds
  )
{
  UINT64  Frequency;
  UINT64  Start;
  UINT64  Now;
  UINT32  Polls;

  __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (Frequency));
  if (Frequency == 0) {
    return FALSE;
  }

  __asm__ volatile ("isb\n\tmrs %0, cntpct_el0" : "=r" (Start) :: "memory");
  for (Polls = 0; Polls < A04BR3_DELAY_MAX_POLLS; Polls++) {
    __asm__ volatile ("isb\n\tmrs %0, cntpct_el0" : "=r" (Now) :: "memory");
    if ((Now - Start) * 1000000 >= (UINT64)Microseconds * Frequency) {
      return TRUE;
    }
  }

  // The counter did not advance far enough: bounded, reported as failure.
  return FALSE;
}

STATIC
VOID
A04BR3Cpu1300 (
  VOID
  )
{
  UINT32  Mux;
  UINT32  Con0;
  UINT32  Con1;
  UINT32  Ctrl;
  UINT32  En;
  UINT32  Vosel;
  UINT32  VoselOn;
  UINT32  Reg;
  UINT32  Orig;
  UINT32  Code;
  UINT32  Check;
  UINT32  Pcw;
  UINT32  FhEn;
  UINT32  Other;
  UINT32  OtherOrig;
  UINT32  OtherCheck;

  Mux  = MmioRead32 (A04BR3_INFRA_MUX);
  FhEn = MmioRead32 (A04BR3_FHCTL_HP_EN);
  Con0 = MmioRead32 (A04BR3_ARMPLL_CON0);
  Con1 = MmioRead32 (A04BR3_ARMPLL_CON1);
  if ((((Mux >> 2) & 3) != 1) || ((Con0 & BIT0) == 0) ||
      ((Con1 & (A04BR3_ARMPLL_PD_MASK | A04BR3_ARMPLL_PCW_MASK)) != A04BR3_ARMPLL_PCW_1001))
  {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: unexpected clock mux=%08x pll0=%08x pll1=%08x\n", Mux, Con0, Con1));
    return;
  }

  if ((FhEn & BIT0) != 0) {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: FHCTL owns ARMPLL (hp_en=%08x)\n", FhEn));
    return;
  }

  // The delay must work before VPROC is touched.
  if (!A04BR3DelayUs (1)) {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: generic timer not running\n"));
    return;
  }

  if (!A04BR3PmicRead (A04BR3_VPROC_CTRL, &Ctrl) || !A04BR3PmicRead (A04BR3_VPROC_EN, &En) ||
      !A04BR3PmicRead (A04BR3_VPROC_VOSEL, &Vosel) || !A04BR3PmicRead (A04BR3_VPROC_VOSEL_ON, &VoselOn))
  {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: VPROC read failed\n"));
    return;
  }

  Reg  = ((Ctrl & BIT1) != 0) ? A04BR3_VPROC_VOSEL_ON : A04BR3_VPROC_VOSEL;
  Orig = (Reg == A04BR3_VPROC_VOSEL_ON) ? VoselOn : Vosel;
  Code = Orig & A04BR3_VPROC_CODE_MASK;
  Other     = (Reg == A04BR3_VPROC_VOSEL_ON) ? A04BR3_VPROC_VOSEL : A04BR3_VPROC_VOSEL_ON;
  OtherOrig = (Reg == A04BR3_VPROC_VOSEL_ON) ? Vosel : VoselOn;
  DEBUG ((
    DEBUG_ERROR,
    "A04BR3 SEC: cpu-1300 before: armpll pcw=%05x (%u MHz) vproc ctrl=%04x en=%04x vosel=%04x vosel_on=%04x active=%a %u mV\n",
    Con1 & A04BR3_ARMPLL_PCW_MASK, (UINT32)(((UINT64)(Con1 & A04BR3_ARMPLL_PCW_MASK) * 26) >> 14),
    Ctrl, En, Vosel, VoselOn, (Reg == A04BR3_VPROC_VOSEL_ON) ? "VOSEL_ON" : "VOSEL", A04BR3VprocMv (Code)
    ));
  if (((En & BIT0) == 0) || (Code < A04BR3_VPROC_CODE_1150) || (Code > A04BR3_VPROC_CODE_1300)) {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: VPROC disabled or outside the stock 1150..1300 mV\n"));
    return;
  }

  // 1. Voltage first: the active selector, then the other one, so that a later
  //    VOSEL_CTRL change cannot select a lower voltage at 1300 MHz.
  if (!A04BR3PmicWrite (Reg, (Orig & ~A04BR3_VPROC_CODE_MASK) | A04BR3_VPROC_CODE_1300) ||
      !A04BR3DelayUs (100) ||
      !A04BR3PmicRead (Reg, &Check) || ((Check & A04BR3_VPROC_CODE_MASK) != A04BR3_VPROC_CODE_1300) ||
      !A04BR3PmicWrite (Other, (OtherOrig & ~A04BR3_VPROC_CODE_MASK) | A04BR3_VPROC_CODE_1300) ||
      !A04BR3PmicRead (Other, &OtherCheck) || ((OtherCheck & A04BR3_VPROC_CODE_MASK) != A04BR3_VPROC_CODE_1300))
  {
    A04BR3PmicWrite (Other, OtherOrig);
    A04BR3PmicWrite (Reg, Orig);
    Check = OtherCheck = 0xFFFFFFFF;
    A04BR3PmicRead (Reg, &Check);
    A04BR3PmicRead (Other, &OtherCheck);
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: VPROC did not read back 1300 mV; restored active=%04x other=%04x, read %04x %04x\n",
            Orig, OtherOrig, Check, OtherCheck));
    return;
  }

  // 2. Then the clock, as mtk_pll_set_rate_regs: PCW, then PCW_CHG, then 20 us.
  Pcw = (Con1 & ~(A04BR3_ARMPLL_PD_MASK | A04BR3_ARMPLL_PCW_MASK | A04BR3_ARMPLL_PCW_CHG)) | A04BR3_ARMPLL_PCW_1300;
  MmioWrite32 (A04BR3_ARMPLL_CON1, Pcw);
  ArmDataSynchronizationBarrier ();
  MmioWrite32 (A04BR3_ARMPLL_CON1, MmioRead32 (A04BR3_ARMPLL_CON1) | A04BR3_ARMPLL_PCW_CHG);
  ArmDataSynchronizationBarrier ();
  if (!A04BR3DelayUs (20)) {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300: 20 us delay incomplete after PCW_CHG\n"));
  }

  Con1 = MmioRead32 (A04BR3_ARMPLL_CON1);
  Check = OtherCheck = 0xFFFFFFFF;
  A04BR3PmicRead (Reg, &Check);
  A04BR3PmicRead (Other, &OtherCheck);
  DEBUG ((
    DEBUG_ERROR,
    "A04BR3 SEC: cpu-1300 after: armpll pll1=%08x pcw=%05x (%u MHz) vproc %a=%04x %u mV other=%04x hp_en=%08x (register values; measure the clock with the PMU)\n",
    Con1, Con1 & A04BR3_ARMPLL_PCW_MASK, (UINT32)(((UINT64)(Con1 & A04BR3_ARMPLL_PCW_MASK) * 26) >> 14),
    (Reg == A04BR3_VPROC_VOSEL_ON) ? "VOSEL_ON" : "VOSEL", Check, A04BR3VprocMv (Check & A04BR3_VPROC_CODE_MASK),
    OtherCheck, MmioRead32 (A04BR3_FHCTL_HP_EN)
    ));
}
#endif


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
#if A04BR3_CPU_STAGE_DIAG == 1 && !defined(MDEPKG_NDEBUG)
  if (Flag) {
    CONST CHAR8 *Stage = "SEC";
    A04BR3_CPU_STAGE Cpu = A04BR3CpuStageRead (MmioRead32);
    DEBUG ((DEBUG_ERROR, "A04BR3 cpu-stage %a: mux=%08x dcm=%08x pll0=%08x pll1=%08x pwr=%08x busctl=%08x bus0=%08x bus1=%08x (non-atomic)\n",
            Stage, Cpu.InfraMux, Cpu.ArmDcm, Cpu.ArmPllCon0, Cpu.ArmPllCon1, Cpu.ArmPllPower,
            Cpu.BusDcmControl, Cpu.BusDcm0, Cpu.BusDcm1));
  }
#endif
  DEBUG ((
    DEBUG_ERROR,
    "A04BR3 SEC: test cycle: fastboot flag %a, watchdog armed for %u s\n",
    Flag ? "set" : "NOT set",
    A04BR3_WATCHDOG_SECONDS
    ));
  if (!Flag) {
    // Do not deliberately halt into an unverified normal-boot reset loop.
    // The already armed WDT remains active; continue the normal boot path,
    // whose existing kernel AutoReset retries RTC setup later.
    DEBUG ((DEBUG_ERROR, "A04BR3 recovery: SEC RTC unverified; smoke refused, normal boot continues\n"));
  }

#if A04BR3_CPU_1300 == 1
  if (Flag) {
    A04BR3Cpu1300 ();
  } else {
    DEBUG ((DEBUG_ERROR, "A04BR3 SEC: cpu-1300 refused: recovery not armed\n"));
  }
#endif

#if A04BR3_EARLY_FAULT == 1
  if (Flag) {
    DEBUG ((DEBUG_ERROR, "A04BR3 RECOVERY SMOKE: SEC armed/RTC verified; deadloop without feeding\n"));
    CpuDeadLoop ();
  }
#endif
}
