/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_RTC_COUNTER_CLOCK_H
#define A04_RTC_COUNTER_CLOCK_H
/* Only a validated MT6392 calendar may seed this continuous-S0 clock.
 * It is not a PMIC read, persistent clock setting, or suspend compensation. */
STATIC EFI_TIME mCounterAnchor;
STATIC UINT64 mCounterTicks,mCounterLastTicks;
STATIC BOOLEAN mCounterValid,mCounterProgressVerified,mCounterUnsupported,mRtcRuntimeAdopted;
STATIC UINT32 RtcMonthDays(UINT32 Year,UINT32 Month)
{
  STATIC CONST UINT8 Days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  return Days[Month-1]+(Month==2 && Year%4==0 && (Year%100!=0 || Year%400==0));
}
STATIC UINT64 RtcReadCounter(VOID)
{
  /* Linux ARM arch_timer.h arch_counter_get_cntpct orders the CP15 read
   * with ISB. Keep this in the board RTC caller, not the shared ArmLib. */
  ArmInstructionSynchronizationBarrier();
  return ArmReadCntPct();
}
STATIC BOOLEAN CounterAdvances(UINT64 *Count)
{
  UINT64 First=RtcReadCounter(),Next;UINTN i;
  for(i=0;i<64;++i){Next=RtcReadCounter();if(Next<First)return FALSE;if(Next>First){*Count=Next;return TRUE;}}
  return FALSE;
}
STATIC EFI_STATUS RtcCounterSeed(CONST EFI_TIME *Time)
{
  mCounterValid=FALSE;mCounterProgressVerified=FALSE;mCounterUnsupported=FALSE;
  /* MT8167 Linux mt8167.dtsi advertises the architectural counter at 13MHz.
   * Read physical count, not CNTVCT whose offset belongs to the OS. */
  if(ArmReadCntFrq()!=13000000U)return EFI_DEVICE_ERROR;
  if(!CounterAdvances(&mCounterTicks))return EFI_DEVICE_ERROR;
  mCounterLastTicks=mCounterTicks;mCounterProgressVerified=TRUE;CopyMem(&mCounterAnchor,Time,sizeof(*Time));
  ArmDataMemoryBarrier();mCounterValid=TRUE;return EFI_SUCCESS;
}
STATIC EFI_STATUS RtcCounterGet(EFI_TIME *Time)
{
  EFI_TIME Result;UINT64 Count,Seconds,DaySeconds;UINT32 Days,Year,Month,Day,Step;
  if(mCounterUnsupported)return EFI_UNSUPPORTED;
  if(!mCounterValid)return EFI_DEVICE_ERROR;
  ArmDataMemoryBarrier();
  if(ArmReadCntFrq()!=13000000U){mCounterValid=FALSE;mCounterUnsupported=TRUE;return EFI_UNSUPPORTED;}
  if(!CounterAdvances(&Count) || Count<mCounterLastTicks){mCounterValid=FALSE;mCounterUnsupported=TRUE;return EFI_UNSUPPORTED;}
  mCounterLastTicks=Count;Seconds=(Count-mCounterTicks)/13000000U;
  /* Never extrapolate outside the PMIC's 1968..2095 supported calendar. */
  if(Seconds>128ULL*366U*86400U)return EFI_DEVICE_ERROR;
  DaySeconds=(UINT64)mCounterAnchor.Hour*3600U+mCounterAnchor.Minute*60U+mCounterAnchor.Second+Seconds;
  Days=(UINT32)(DaySeconds/86400U);DaySeconds%=86400U;
  Year=mCounterAnchor.Year;Month=mCounterAnchor.Month;Day=mCounterAnchor.Day;
  while(Days) {
    Step=RtcMonthDays(Year,Month)-Day+1U;
    if(Days<Step){Day+=Days;break;}
    Days-=Step;Day=1;
    if(++Month==13){Month=1;if(++Year>2095)return EFI_DEVICE_ERROR;}
  }
  CopyMem(&Result,&mCounterAnchor,sizeof(Result));Result.Year=(UINT16)Year;
  Result.Month=(UINT8)Month;Result.Day=(UINT8)Day;Result.Hour=(UINT8)(DaySeconds/3600U);
  Result.Minute=(UINT8)((DaySeconds%3600U)/60U);Result.Second=(UINT8)(DaySeconds%60U);
  /* One-second reporting: do not invent fractional phase for an integer RTC seed. */
  Result.Nanosecond=0;CopyMem(Time,&Result,sizeof(Result));return EFI_SUCCESS;
}
#endif
