/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT6392 boot-only, read-only calendar. Linux rtc-mt6397.c + mt6397-core.c.
 * Never access the shared WACS2 channel after the OS owns PWRAP. */
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Guid/RtPropertiesTable.h>
#ifndef A04BR3_MT6392_RTC_RUNTIME_COUNTER
#define A04BR3_MT6392_RTC_RUNTIME_COUNTER 0
#endif
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
#include <Library/ArmLib.h>
#include <Guid/EventGroup.h>
#include "RtcCounterClock.h"
STATIC EFI_EVENT mBeforeExitEvent;
#endif
#include "../../Include/PwrapRead.h"
#define RTC_BBPU 0x8000U
#define RTC_TC_SEC 0x800aU
#define RTC_CBUSY (1U<<6)
#ifndef A04BR3_RTC_ACCURACY_X1E6_PPM
#define A04BR3_RTC_ACCURACY_X1E6_PPM 0
#endif
#if A04BR3_RTC_ACCURACY_X1E6_PPM < 0 || A04BR3_RTC_ACCURACY_X1E6_PPM > 0xffffffffU
#error Invalid declared RTC clock accuracy rating
#endif
STATIC BOOLEAN mLogged,mRtcCalendarValid,mRtcTcChangeObserved;
STATIC VOID FillCapabilities(EFI_TIME_CAPABILITIES *Caps)
{
  if(Caps){Caps->Resolution=1;Caps->Accuracy=(UINT32)A04BR3_RTC_ACCURACY_X1E6_PPM;Caps->SetsToZero=FALSE;}
}
STATIC EFI_RT_PROPERTIES_TABLE mProperties;
STATIC BOOLEAN CalendarValid(CONST UINT16 *v)
{
  STATIC CONST UINT8 Days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  UINT32 Year=1968U+v[6], Limit;
  if(v[0]>59 || v[1]>59 || v[2]>23 || v[5]<1 || v[5]>12 || v[6]>127)return FALSE;
  /* EFI_TIME has no DOW. Linux also recomputes weekday instead of trusting
   * the RTC DOW register; retain it only in the raw log. */
  Limit=Days[v[5]-1];
  if(v[5]==2 && (Year%4==0 && (Year%100!=0 || Year%400==0)))++Limit;
  return v[3]>=1 && v[3]<=Limit;
}
STATIC EFI_STATUS BootGetTime(EFI_TIME *Time,EFI_TIME_CAPABILITIES *Capabilities,BOOLEAN Adopt)
{
  EFI_STATUS Status=EFI_NOT_READY;EFI_TPL OldTpl;UINT16 Raw[7]={0},Bbpu=0,After=0;UINTN Attempt=0,i;
  EFI_TIME Result;
#if !A04BR3_MT6392_RTC_RUNTIME_COUNTER
  (VOID)Adopt;
#endif
  if(Time==NULL)return EFI_INVALID_PARAMETER;
  /* Zero is our build-time unqualified marker, never an EFI accuracy value.
   * A nonzero rating needs evidence for this complete RTC + counter backend. */
  if(Capabilities!=NULL && A04BR3_RTC_ACCURACY_X1E6_PPM==0)return EFI_UNSUPPORTED;
  mRtcCalendarValid=FALSE;OldTpl=gBS->RaiseTPL(TPL_HIGH_LEVEL);
  if((MmioRead32(WACS_DATA)&BIT21)==0)goto Done;
  for(Attempt=0;Attempt<3;++Attempt) {
    Status=ReadPmic(RTC_BBPU,&Bbpu);if(EFI_ERROR(Status))break;
    if(Bbpu&RTC_CBUSY){Status=EFI_NOT_READY;break;}
    for(i=0;i<7;++i){Status=ReadPmic((UINT16)(RTC_TC_SEC+2*i),&Raw[i]);if(EFI_ERROR(Status))break;}
    if(EFI_ERROR(Status))break;
    Status=ReadPmic(RTC_TC_SEC,&After);if(EFI_ERROR(Status))break;
    if(After<=59 && Raw[0]<=59 && After!=Raw[0])mRtcTcChangeObserved=TRUE;
    if(After<Raw[0]){Status=EFI_NOT_READY;continue;} /* Linux rollover retry. */
    if(After>59 || !CalendarValid(Raw)){Status=EFI_DEVICE_ERROR;break;}
    Status=ReadPmic(RTC_BBPU,&Bbpu);if(EFI_ERROR(Status))break;
    if(Bbpu&RTC_CBUSY){Status=EFI_NOT_READY;break;}
    mRtcCalendarValid=TRUE;ZeroMem(&Result,sizeof(Result));
    Result.Year=(UINT16)(1968U+Raw[6]);Result.Month=(UINT8)Raw[5];Result.Day=(UINT8)Raw[3];
    Result.Hour=(UINT8)Raw[2];Result.Minute=(UINT8)Raw[1];Result.Second=(UINT8)Raw[0];
    Result.TimeZone=EFI_UNSPECIFIED_TIMEZONE; /* RTC stores no timezone. */
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
    if(Adopt){Status=RtcCounterSeed(&Result);if(EFI_ERROR(Status))break;}
#endif
    CopyMem(Time,&Result,sizeof(Result));FillCapabilities(Capabilities);Status=EFI_SUCCESS;break;
  }
Done:
  gBS->RestoreTPL(OldTpl);
  if(!mLogged) {
    mLogged=TRUE;
    DEBUG((DEBUG_ERROR,"A04RTC boot-read status=%r bbpu=%04x tc=%04x/%04x/%04x/%04x/%04x/%04x/%04x sec2=%04x attempt=%u rtc_calendar_valid=%u tc_change_observed=%u loss_validity=unknown\n",
      Status,Bbpu,Raw[0],Raw[1],Raw[2],Raw[3],Raw[4],Raw[5],Raw[6],After,(UINT32)Attempt,(UINT32)mRtcCalendarValid,(UINT32)mRtcTcChangeObserved));
  }
  /* Transport / unready / incoherent / invalid calendar never returns success. */
  return EFI_ERROR(Status)?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
EFI_STATUS EFIAPI LibGetTime(EFI_TIME *Time,EFI_TIME_CAPABILITIES *Capabilities)
{
  if(Time==NULL)return EFI_INVALID_PARAMETER;
  /* Public runtime entry never reaches PWRAP or boot services. */
  if(EfiAtRuntime()) {
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
    if(Capabilities!=NULL && A04BR3_RTC_ACCURACY_X1E6_PPM==0)return EFI_UNSUPPORTED;
    if(!mRtcRuntimeAdopted)return EFI_DEVICE_ERROR;
    EFI_STATUS Status=RtcCounterGet(Time);
    if(!EFI_ERROR(Status))FillCapabilities(Capabilities);
    return Status;
#else
    return EFI_UNSUPPORTED;
#endif
  }
  return BootGetTime(Time,Capabilities,TRUE);
}
EFI_STATUS EFIAPI LibSetTime(EFI_TIME *Time)
{
  STATIC CONST UINT8 Days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  UINT32 Limit;EFI_TIME Current;EFI_STATUS Status;
  if(!Time || Time->Year<1900 || Time->Year>9999 || Time->Month<1 || Time->Month>12)return EFI_INVALID_PARAMETER;
  Limit=Days[Time->Month-1]+(Time->Month==2 && Time->Year%4==0 && (Time->Year%100!=0 || Time->Year%400==0));
  if(Time->Day<1 || Time->Day>Limit || Time->Hour>23 || Time->Minute>59 || Time->Second>59 ||
     Time->Nanosecond>999999999U || Time->Pad1 || Time->Pad2 ||
     (Time->TimeZone!=EFI_UNSPECIFIED_TIMEZONE && (Time->TimeZone<-1440 || Time->TimeZone>1440)) ||
     (Time->Daylight&~3U))return EFI_INVALID_PARAMETER;
  /* A counter-only change would falsely claim persistent RTC setting.
   * Real writes/rollback of a partial TC transaction are not qualified. */
  if(EfiAtRuntime() || Time->Year<1968 || Time->Year>2095 || Time->Nanosecond || Time->TimeZone!=EFI_UNSPECIFIED_TIMEZONE || Time->Daylight)return EFI_UNSUPPORTED;
  Status=BootGetTime(&Current,NULL,FALSE);if(EFI_ERROR(Status))return Status;
  if(Current.Year!=Time->Year || Current.Month!=Time->Month || Current.Day!=Time->Day ||
     Current.Hour!=Time->Hour || Current.Minute!=Time->Minute || Current.Second!=Time->Second)return EFI_UNSUPPORTED;
  return EFI_SUCCESS; /* Read-verified no-op only. No PMIC or anchor write. */
}
EFI_STATUS EFIAPI LibGetWakeupTime(BOOLEAN *Enabled,BOOLEAN *Pending,EFI_TIME *Time)
{return !Enabled || !Pending || !Time?EFI_INVALID_PARAMETER:EFI_UNSUPPORTED;}
EFI_STATUS EFIAPI LibSetWakeupTime(BOOLEAN Enabled,EFI_TIME *Time)
{return Enabled && !Time?EFI_INVALID_PARAMETER:EFI_UNSUPPORTED;}
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
STATIC VOID EFIAPI RtcBeforeExit(EFI_EVENT Event,VOID *Context)
{
  EFI_TIME Last;EFI_STATUS Status;
  (VOID)Event;(VOID)Context;
  /* Refresh from real TC immediately before handoff, while boot still owns
   * WACS2. A failed final snapshot invalidates the runtime clock. */
  mCounterValid=FALSE;mCounterProgressVerified=FALSE;mRtcRuntimeAdopted=FALSE;
  /* This event is a boot-services ownership boundary, not the runtime API.
   * A prior failed EBS attempt may already have set UefiRuntimeLib's flag. */
  Status=BootGetTime(&Last,NULL,TRUE);
  if(EFI_ERROR(Status))mCounterValid=FALSE;
  else {ArmDataMemoryBarrier();mRtcRuntimeAdopted=TRUE;}
  DEBUG((DEBUG_ERROR,"A04RTC counter-handoff status=%r seed_adopted=%u rtc_calendar_valid=%u tc_change_observed=%u counter_alive=%u source=MT6392+CNTPCT live_pmic_runtime=0 suspend_compensation=0\n",Status,(UINT32)mRtcRuntimeAdopted,(UINT32)mRtcCalendarValid,(UINT32)mRtcTcChangeObserved,(UINT32)mCounterProgressVerified));
}
#endif
EFI_STATUS EFIAPI LibRtcInitialize(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;EFI_RT_PROPERTIES_TABLE *Previous=NULL;EFI_TIME Probe;
  (VOID)ImageHandle;(VOID)SystemTable;
  /* Preserve the prior explicit table or its implicit all-supported default.
   * Only remove the four time services at runtime; this does not assess or
   * change the remaining runtime services. The table is resident in the RTC
   * runtime image, not on this stack or in boot-services allocated memory. */
  Status=EfiGetSystemConfigurationTable(&gEfiRtPropertiesTableGuid,(VOID **)&Previous);
  if(Status!=EFI_NOT_FOUND && EFI_ERROR(Status))return Status;
  mProperties.Version=EFI_RT_PROPERTIES_TABLE_VERSION;mProperties.Length=sizeof(mProperties);
  if(!EFI_ERROR(Status)) {
    if(!Previous || Previous->Version!=EFI_RT_PROPERTIES_TABLE_VERSION || Previous->Length!=sizeof(*Previous))return EFI_UNSUPPORTED;
    mProperties.RuntimeServicesSupported=Previous->RuntimeServicesSupported;
  } else mProperties.RuntimeServicesSupported=0x3fffU;
  mProperties.RuntimeServicesSupported&=~(EFI_RT_SUPPORTED_GET_TIME|EFI_RT_SUPPORTED_SET_TIME|
    EFI_RT_SUPPORTED_GET_WAKEUP_TIME|EFI_RT_SUPPORTED_SET_WAKEUP_TIME);
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
  mProperties.RuntimeServicesSupported|=EFI_RT_SUPPORTED_GET_TIME;
  Status=gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,RtcBeforeExit,NULL,
    &gEfiEventBeforeExitBootServicesGuid,&mBeforeExitEvent);
  if(EFI_ERROR(Status))return Status;
#endif
  Status=gBS->InstallConfigurationTable(&gEfiRtPropertiesTableGuid,&mProperties);
  if(EFI_ERROR(Status)) {
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
    gBS->CloseEvent(mBeforeExitEvent);mBeforeExitEvent=NULL;mCounterValid=FALSE;
#endif
    return Status;
  }
  /* Driver registration must succeed even if the clock is unavailable; the
   * real GetTime then fails rather than leaving a fabricated default clock. */
  (VOID)LibGetTime(&Probe,NULL);
  return EFI_SUCCESS;
}
