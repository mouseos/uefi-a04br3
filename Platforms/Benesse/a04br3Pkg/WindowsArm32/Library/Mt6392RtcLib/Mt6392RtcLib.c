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
#include "../../Include/PwrapRead.h"
#define RTC_BBPU 0x8000U
#define RTC_TC_SEC 0x800aU
#define RTC_CBUSY (1U<<6)
STATIC BOOLEAN mLogged;
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
EFI_STATUS EFIAPI LibGetTime(EFI_TIME *Time,EFI_TIME_CAPABILITIES *Capabilities)
{
  EFI_STATUS Status=EFI_NOT_READY;EFI_TPL OldTpl;UINT16 Raw[7]={0},Bbpu=0,After=0;UINTN Attempt=0,i;
  EFI_TIME Result;
  if(Time==NULL)return EFI_INVALID_PARAMETER;
  /* No boot-service calls, MMIO or virtual physical pointers on this path. */
  if(EfiAtRuntime())return EFI_UNSUPPORTED;
  /* No verified MT6392 oscillator error bound is available. Do not claim
   * Accuracy=0 (perfect) or fabricate a ppm rating. Calendar-only subset. */
  if(Capabilities!=NULL)return EFI_UNSUPPORTED;
  OldTpl=gBS->RaiseTPL(TPL_HIGH_LEVEL);
  if((MmioRead32(WACS_DATA)&BIT21)==0)goto Done;
  for(Attempt=0;Attempt<3;++Attempt) {
    Status=ReadPmic(RTC_BBPU,&Bbpu);if(EFI_ERROR(Status))break;
    if(Bbpu&RTC_CBUSY){Status=EFI_NOT_READY;break;}
    for(i=0;i<7;++i){Status=ReadPmic((UINT16)(RTC_TC_SEC+2*i),&Raw[i]);if(EFI_ERROR(Status))break;}
    if(EFI_ERROR(Status))break;
    Status=ReadPmic(RTC_TC_SEC,&After);if(EFI_ERROR(Status))break;
    if(After<Raw[0]){Status=EFI_NOT_READY;continue;} /* Linux rollover retry. */
    if(After>59 || !CalendarValid(Raw)){Status=EFI_DEVICE_ERROR;break;}
    Status=ReadPmic(RTC_BBPU,&Bbpu);if(EFI_ERROR(Status))break;
    if(Bbpu&RTC_CBUSY){Status=EFI_NOT_READY;break;}
    ZeroMem(&Result,sizeof(Result));
    Result.Year=(UINT16)(1968U+Raw[6]);Result.Month=(UINT8)Raw[5];Result.Day=(UINT8)Raw[3];
    Result.Hour=(UINT8)Raw[2];Result.Minute=(UINT8)Raw[1];Result.Second=(UINT8)Raw[0];
    Result.TimeZone=EFI_UNSPECIFIED_TIMEZONE; /* RTC stores no timezone. */
    CopyMem(Time,&Result,sizeof(Result));Status=EFI_SUCCESS;break;
  }
Done:
  gBS->RestoreTPL(OldTpl);
  if(!mLogged) {
    mLogged=TRUE;
    DEBUG((DEBUG_ERROR,"A04RTC boot-read status=%r bbpu=%04x tc=%04x/%04x/%04x/%04x/%04x/%04x/%04x sec2=%04x attempt=%u calendar_only=1 loss_validity=unknown\n",
      Status,Bbpu,Raw[0],Raw[1],Raw[2],Raw[3],Raw[4],Raw[5],Raw[6],After,(UINT32)Attempt));
  }
  /* Transport / unready / incoherent / invalid calendar never returns success. */
  return EFI_ERROR(Status)?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
EFI_STATUS EFIAPI LibSetTime(EFI_TIME *Time)
{return Time==NULL?EFI_INVALID_PARAMETER:EFI_UNSUPPORTED;}
EFI_STATUS EFIAPI LibGetWakeupTime(BOOLEAN *Enabled,BOOLEAN *Pending,EFI_TIME *Time)
{return !Enabled || !Pending || !Time?EFI_INVALID_PARAMETER:EFI_UNSUPPORTED;}
EFI_STATUS EFIAPI LibSetWakeupTime(BOOLEAN Enabled,EFI_TIME *Time)
{return Enabled && !Time?EFI_INVALID_PARAMETER:EFI_UNSUPPORTED;}
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
  Status=gBS->InstallConfigurationTable(&gEfiRtPropertiesTableGuid,&mProperties);
  if(EFI_ERROR(Status))return Status;
  /* Driver registration must succeed even if the clock is unavailable; the
   * real GetTime then fails rather than leaving a fabricated default clock. */
  (VOID)LibGetTime(&Probe,NULL);
  return EFI_SUCCESS;
}
