/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * CPU 1300 MHz transition for the Windows ARM32 PrePi, same sequence as the
 * hardware-verified AArch64 SEC switch (A04BR3PlatformSecBoardLib.c, measured
 * 1300 MHz with PMU cycles on 2026-10-31). Values from the stock CT3 kernel-4.4:
 * mt8167.dtsi cluster0_opp 1300 MHz at 1.30 V, mt6392 VPROC VOSEL/VOSEL_ON
 * 0.7 V + 6.25 mV/code, ARMPLL CON1 PCW = MHz/26*2^14 then PCW_CHG, 20 us.
 * Only the known LK state (mux ARMPLL, 1001 MHz, VPROC 1.15..1.30 V, FHCTL
 * not owning ARMPLL) is changed. RTC_SPAR0 bit 12 guards re-entry.
 * Result: 0 switched to 1300 MHz, 1 left unchanged at the LK clock. */
#ifndef A04BR3_CPU1300_CORE_H
#define A04BR3_CPU1300_CORE_H
typedef struct {
  unsigned int (*read)(unsigned int);
  void (*write)(unsigned int,unsigned int);
  int (*pmic_read)(unsigned int,unsigned int *);
  int (*pmic_write)(unsigned int,unsigned int);
  int (*guard)(unsigned int clear,unsigned int set,unsigned int *value);
  int (*delay)(unsigned int us);
} CPU1300_IO;
#define C13_INFRA_MUX   0x10001000U
#define C13_PLL_CON0    0x10018100U
#define C13_PLL_CON1    0x10018104U
#define C13_FH_HP_EN    0x10018014U
#define C13_PCW_MASK    0x001FFFFFU
#define C13_PD_MASK     (0x7U<<24)
#define C13_PCW_CHG     0x80000000U
#define C13_PCW_1001    0x0009A000U
#define C13_PCW_1300    0x000C8000U
#define C13_VCTRL       0x0216
#define C13_VEN         0x021A
#define C13_VOSEL       0x021E
#define C13_VOSEL_ON    0x0220
#define C13_CODE_MASK   0x7FU
#define C13_CODE_1150   0x48U
#define C13_CODE_1300   0x60U
#define C13_PENDING     0x1000U
static int Cpu1300GuardClear(const CPU1300_IO *io,unsigned int *spare)
{ int t; for(t=0;t<3;t++) if(io->guard(C13_PENDING,0,spare)) return 1; return 0; }
static int Cpu1300Transition(const CPU1300_IO *io)
{
  unsigned int mux,con0,con1,fh,spare=0,ctrl,en,vosel,voselOn,reg,orig,code,other,otherOrig,check,otherCheck;
  mux=io->read(C13_INFRA_MUX);fh=io->read(C13_FH_HP_EN);con0=io->read(C13_PLL_CON0);con1=io->read(C13_PLL_CON1);
  DEBUG((DEBUG_ERROR,"A04 CPU1300 before: mux=%08x pll0=%08x pll1=%08x hp_en=%08x\n",mux,con0,con1,fh));
  if(((mux>>2)&3U)!=1U || !(con0&1U) || (con1&(C13_PD_MASK|C13_PCW_MASK))!=C13_PCW_1001) {
    DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: unexpected clock state\n"));return 1; }
  if(fh&1U){DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: FHCTL owns ARMPLL\n"));return 1;}
  if(!io->delay(1)){DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: generic timer not running\n"));return 1;}
  if(!io->guard(0,0,&spare)){DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: RTC_SPAR0 unreadable\n"));return 1;}
  if(spare&C13_PENDING){
    /* A previous attempt did not finish. Clear the mark (the bounded watchdog
     * returns a hang to fastboot) and stay at the LK clock for this boot. */
    int cleared=Cpu1300GuardClear(io,&spare);
    DEBUG((DEBUG_ERROR,"A04 CPU1300 skipped: previous switch unfinished; guard %a (spar0=%04x)\n",cleared?"cleared":"kept",spare));
    return 1; }
  if(!io->pmic_read(C13_VCTRL,&ctrl)||!io->pmic_read(C13_VEN,&en)||!io->pmic_read(C13_VOSEL,&vosel)||!io->pmic_read(C13_VOSEL_ON,&voselOn)){
    DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: VPROC read failed\n"));return 1; }
  reg=(ctrl&2U)?C13_VOSEL_ON:C13_VOSEL;orig=(reg==C13_VOSEL_ON)?voselOn:vosel;code=orig&C13_CODE_MASK;
  other=(reg==C13_VOSEL_ON)?C13_VOSEL:C13_VOSEL_ON;otherOrig=(reg==C13_VOSEL_ON)?vosel:voselOn;
  DEBUG((DEBUG_ERROR,"A04 CPU1300 vproc ctrl=%04x en=%04x vosel=%04x vosel_on=%04x active %u mV\n",ctrl,en,vosel,voselOn,700U+code*625U/100U));
  if(!(en&1U) || code<C13_CODE_1150 || code>C13_CODE_1300){DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: VPROC outside stock range\n"));return 1;}
  if(!io->guard(0,C13_PENDING,&spare)){ (void)Cpu1300GuardClear(io,&spare);
    DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: guard not set\n"));return 1; }
  if(!io->pmic_write(reg,(orig&~C13_CODE_MASK)|C13_CODE_1300) || !io->delay(100) ||
     !io->pmic_read(reg,&check) || (check&C13_CODE_MASK)!=C13_CODE_1300 ||
     !io->pmic_write(other,(otherOrig&~C13_CODE_MASK)|C13_CODE_1300) ||
     !io->pmic_read(other,&otherCheck) || (otherCheck&C13_CODE_MASK)!=C13_CODE_1300) {
    int cleared=0;
    io->pmic_write(other,otherOrig);io->pmic_write(reg,orig);check=otherCheck=0xFFFFFFFFU;
    io->pmic_read(reg,&check);io->pmic_read(other,&otherCheck);
    if(check==orig && otherCheck==otherOrig) cleared=Cpu1300GuardClear(io,&spare);
    DEBUG((DEBUG_ERROR,"A04 CPU1300 refused: VPROC readback failed; restored %04x %04x guard %a\n",check,otherCheck,cleared?"cleared":"kept"));
    return 1; }
  io->write(C13_PLL_CON1,(con1&~(C13_PD_MASK|C13_PCW_MASK|C13_PCW_CHG))|C13_PCW_1300);
  io->write(C13_PLL_CON1,io->read(C13_PLL_CON1)|C13_PCW_CHG);
  if(!io->delay(20)) DEBUG((DEBUG_ERROR,"A04 CPU1300: 20 us delay incomplete\n"));
  con1=io->read(C13_PLL_CON1);check=otherCheck=0xFFFFFFFFU;io->pmic_read(reg,&check);io->pmic_read(other,&otherCheck);
  if((con1&(C13_PD_MASK|C13_PCW_MASK))==C13_PCW_1300 && (check&C13_CODE_MASK)==C13_CODE_1300) {
    int cleared=Cpu1300GuardClear(io,&spare);
    DEBUG((DEBUG_ERROR,"A04 CPU1300 after: pll1=%08x (1300 MHz) vproc=%04x other=%04x 1300 mV guard %a\n",con1,check,otherCheck,cleared?"cleared":"kept"));
    return 0; }
  DEBUG((DEBUG_ERROR,"A04 CPU1300 readback mismatch: pll1=%08x vproc=%04x guard kept\n",con1,check));
  return 1;
}
#endif
