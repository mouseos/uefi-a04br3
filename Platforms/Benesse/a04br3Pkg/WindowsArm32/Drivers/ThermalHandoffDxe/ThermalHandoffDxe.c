// SPDX-License-Identifier: MIT
// Records only that this driver's BeforeExitBootServices callback ran.
// It does not inspect or stop EL3, LK, the ADC, or other firmware clients.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include "ThermalHandoffRecord.h"
#ifndef A04BR3_THERMAL_PIO_HANDOFF
#define A04BR3_THERMAL_PIO_HANDOFF 0
#endif
#if A04BR3_THERMAL_PIO_HANDOFF
#include "ThermalPioRecord.h"
#include <A04BR3ThermalCal.h>
#include <Library/HobLib.h>
STATIC BOOLEAN mPioCalReady;
VOID A04ThermalPioBeforeExit(VOID);
#endif
STATIC EFI_GUID mRecordGuid={0xb2dd7543,0x9b8a,0x4580,{0xb8,0x22,0x14,0x35,0x09,0xa0,0x47,0xc1}};
STATIC CONST CHAR16 mName[]=L"A04BR3ThermalHandoff";
STATIC VOID EFIAPI RecordBeforeExit(EFI_EVENT Event,VOID *Context)
{
  A04_THERMAL_HANDOFF_RECORD Record=A04_THERMAL_RECORD_UNKNOWN;
  EFI_STATUS Status;
  (VOID)Event;(VOID)Context;
#if A04BR3_THERMAL_PIO_HANDOFF
  if(mPioCalReady) A04ThermalPioBeforeExit();
  else DEBUG((DEBUG_ERROR,"A04 THM PIO calibration unavailable; producer unchanged\n"));
#endif
  Status=gRT->SetVariable((CHAR16 *)mName,&mRecordGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,sizeof(Record),&Record);
  DEBUG((DEBUG_ERROR,"A04 THERMAL: handoff observation=%r; ownership unverified\n",Status));
}
EFI_STATUS EFIAPI ThermalHandoffEntry(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;EFI_STATUS Status;
  (VOID)ImageHandle;(VOID)SystemTable;
#if A04BR3_THERMAL_PIO_HANDOFF
  { EFI_GUID guid=A04_THERMAL_PIO_GUID;
    EFI_GUID calGuid=A04BR3_THERMAL_CAL_GUID;
    VOID *Hob;A04BR3_THERMAL_CAL *Cal;
    Status=gRT->SetVariable(A04_THERMAL_PIO_NAME,&guid,0,0,NULL);
    if(EFI_ERROR(Status) && Status!=EFI_NOT_FOUND)return Status;
    Status=gRT->SetVariable(L"A04BR3ThermalCalibration",&calGuid,0,0,NULL);
    if(EFI_ERROR(Status) && Status!=EFI_NOT_FOUND)return Status;
    Hob=GetFirstGuidHob(&calGuid);
    if(Hob!=NULL && GET_GUID_HOB_DATA_SIZE(Hob)==sizeof(A04BR3_THERMAL_CAL)) {
      Cal=GET_GUID_HOB_DATA(Hob);
      if(Cal->Version==1 && (Cal->Devinfo60&1) && Cal->Devinfo60!=MAX_UINT32 &&
         Cal->Devinfo61!=MAX_UINT32 && Cal->Devinfo62!=MAX_UINT32) {
        Status=gRT->SetVariable(L"A04BR3ThermalCalibration",&calGuid,6,sizeof(*Cal),Cal);
        mPioCalReady=!EFI_ERROR(Status);
        DEBUG((DEBUG_ERROR,"A04 THM PIO real LK calibration publish=%r\n",Status));
      }
    }
  }
#endif
  // Clear any old value; do not silently accept a stale observation.
  Status=gRT->SetVariable((CHAR16 *)mName,&mRecordGuid,0,0,NULL);
  if(EFI_ERROR(Status) && Status!=EFI_NOT_FOUND)return Status;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,RecordBeforeExit,NULL,
                            &gEfiEventBeforeExitBootServicesGuid,&Event);
}
