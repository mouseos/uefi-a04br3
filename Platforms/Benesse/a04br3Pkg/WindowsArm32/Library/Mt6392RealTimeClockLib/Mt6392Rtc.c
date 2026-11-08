/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Boot-only MT6392 read through the existing WACS2 transport. Runtime is
 * immutable calendar seed plus the existing physical architectural counter.
 * No PMIC owner, MMIO, boot/runtime service calls, or NVRAM at GetTime. */
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ArmLib.h>
#include <Library/ArmGenericTimerCounterLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include "../../Include/PwrapRead.h"
#include "Calendar.h"
STATIC BOOLEAN mInitialized,mValid;
STATIC UINT32 mFrequency;
STATIC UINT64 mSeedCounter,mSeedSeconds;
/* Invoked only once by LibRtcInitialize before installing the RTC Arch
 * protocol. HIGH_LEVEL blocks other DXE events using the same boot transport.
 * AP boot workers do not use WACS2. No transport reset/recovery is offered. */
STATIC EFI_STATUS MtSnapshot(UINT64 *seconds,UINT64 *counter,UINT32 frequency){
 UINT16 raw[7],again,busy;UINT32 init;UINTN retry,i;EFI_STATUS status;UINT64 start,end;
 init=MmioRead32(0x1000f09cU);
 if(init==MAX_UINT32||!(init&1))return EFI_NOT_READY;
 start=ArmGenericTimerGetSystemCount();
 for(retry=0;retry<3;++retry){
  status=ReadPmic(0x8000,&busy);if(EFI_ERROR(status))return status;
  if(busy&0x40)return EFI_NOT_READY;
  for(i=0;i<7;++i){status=ReadPmic((UINT16)(0x800a+2*i),&raw[i]);if(EFI_ERROR(status))return status;}
  status=ReadPmic(0x800a,&again);if(EFI_ERROR(status))return status;
  status=ReadPmic(0x8000,&busy);if(EFI_ERROR(status))return status;
  ArmDataSynchronizationBarrier();end=ArmGenericTimerGetSystemCount();
  if(end<start||end-start>=frequency||ArmGenericTimerGetTimerFreq()!=frequency)return EFI_DEVICE_ERROR;
  if(busy&0x40)return EFI_NOT_READY;
  if(again!=raw[0])continue;
  if(!MtCalendarSeconds(raw,seconds))return EFI_DEVICE_ERROR;
  *counter=end;return EFI_SUCCESS;
 }
 return EFI_NOT_READY;
}
EFI_STATUS EFIAPI LibGetTime(OUT EFI_TIME *Time,OUT EFI_TIME_CAPABILITIES *Capabilities){
 UINT64 now,elapsed;EFI_TIME result;
 if(!Time)return EFI_INVALID_PARAMETER;
 /* UEFI Accuracy has no unknown sentinel. Do not return Accuracy0 (perfect)
  * or a made-up bound. This candidate supports time-only queries explicitly. */
 if(Capabilities)return EFI_UNSUPPORTED;
 if(!mValid||!mFrequency||ArmGenericTimerGetTimerFreq()!=mFrequency)return EFI_DEVICE_ERROR;
 now=ArmGenericTimerGetSystemCount();if(now<mSeedCounter)return EFI_DEVICE_ERROR;
 elapsed=DivU64x32(now-mSeedCounter,mFrequency);
 if(elapsed>MAX_UINT64-mSeedSeconds||!MtSecondsCalendar(mSeedSeconds+elapsed,&result))return EFI_DEVICE_ERROR;
 *Time=result;return EFI_SUCCESS;
}
EFI_STATUS EFIAPI LibSetTime(IN EFI_TIME *Time){return Time?EFI_UNSUPPORTED:EFI_INVALID_PARAMETER;}
EFI_STATUS EFIAPI LibGetWakeupTime(OUT BOOLEAN *Enabled,OUT BOOLEAN *Pending,OUT EFI_TIME *Time){
 if(!Enabled||!Pending||!Time) { return EFI_INVALID_PARAMETER; }
 return EFI_UNSUPPORTED;
}
EFI_STATUS EFIAPI LibSetWakeupTime(IN BOOLEAN Enabled,OUT EFI_TIME *Time){
 if(Enabled&&!Time) { return EFI_INVALID_PARAMETER; }
 return EFI_UNSUPPORTED;
}
EFI_STATUS EFIAPI LibRtcInitialize(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable){
 EFI_TPL old;EFI_STATUS status;EFI_TIME check;
 (VOID)ImageHandle;
 if(mInitialized)return EFI_ALREADY_STARTED;
 if(!SystemTable||!gBS)return EFI_INVALID_PARAMETER;
 mInitialized=TRUE;mFrequency=(UINT32)ArmGenericTimerGetTimerFreq();status=EFI_DEVICE_ERROR;
 if(mFrequency){
  old=gBS->RaiseTPL(TPL_HIGH_LEVEL);
  status=MtSnapshot(&mSeedSeconds,&mSeedCounter,mFrequency);
  mValid=!EFI_ERROR(status);
  gBS->RestoreTPL(old);
 }
 DEBUG((DEBUG_ERROR,"MT6392 RTC boot seed status=%r valid=%u; local timezone unspecified; accuracy unqualified\n",status,(UINT32)mValid));
 if(mValid&&!EFI_ERROR(LibGetTime(&check,NULL)))
  DEBUG((DEBUG_ERROR,"MT6392 RTC seed-derived local %04u-%02u-%02u %02u:%02u:%02u; counter-derived, no runtime PMIC\n",check.Year,check.Month,check.Day,check.Hour,check.Minute,check.Second));
 /* Install callable services even for a failed seed: GetTime returns error,
  * never a dummy epoch. BDS must not wait forever for an absent RTC protocol. */
 return EFI_SUCCESS;
}
