// SPDX-License-Identifier: MIT
// Records only that this driver's BeforeExitBootServices callback ran.
// It does not inspect or stop EL3, LK, the ADC, or other firmware clients.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include "ThermalHandoffRecord.h"
#include "AdcObservation.h"
STATIC EFI_GUID mRecordGuid={0xb2dd7543,0x9b8a,0x4580,{0xb8,0x22,0x14,0x35,0x09,0xa0,0x47,0xc1}};
STATIC CONST CHAR16 mName[]=L"A04BR3ThermalHandoff";
STATIC EFI_GUID mAdcGuid={0x91b27b25,0x4cc5,0x4471,{0xa0,0x4b,0x81,0x67,0x03,0x01,0x00,0x01}};
STATIC CONST CHAR16 mAdcName[]=L"A04BR3AdcObservation";
STATIC VOID EFIAPI RecordBeforeExit(EFI_EVENT Event,VOID *Context)
{
  A04_THERMAL_HANDOFF_RECORD Record=A04_THERMAL_RECORD_UNKNOWN;
  EFI_STATUS Status;
  (VOID)Event;(VOID)Context;
  Status=gRT->SetVariable((CHAR16 *)mName,&mRecordGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,sizeof(Record),&Record);
  DEBUG((DEBUG_ERROR,"A04 THERMAL: handoff observation=%r; ownership unverified\n",Status));
}
EFI_STATUS EFIAPI ThermalHandoffEntry(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;EFI_STATUS Status;UINT32 First,Second;A04_ADC_OBSERVATION Observation;
  (VOID)ImageHandle;(VOID)SystemTable;
  // Clear any old value; do not silently accept a stale observation.
  Status=gRT->SetVariable((CHAR16 *)mName,&mRecordGuid,0,0,NULL);
  if(EFI_ERROR(Status) && Status!=EFI_NOT_FOUND)return Status;
  /* This is early DXE, not ExitBootServices. Never claim that other firmware
   * or EL3 released the ADC; do not touch the ADC registers or clocks. */
  Status=gRT->SetVariable((CHAR16 *)mAdcName,&mAdcGuid,0,0,NULL);
  if(EFI_ERROR(Status) && Status!=EFI_NOT_FOUND)return Status;
  First=MmioRead32(0x10000024u);Second=MmioRead32(0x10000024u);
  if(!A04AdcObservationBuild(First,Second,&Observation)) {
    Status=gRT->SetVariable((CHAR16 *)mAdcName,&mAdcGuid,
      EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,sizeof(Observation),&Observation);
    DEBUG((DEBUG_ERROR,"A04 ADC: early clock observation=%r; secure release unknown\n",Status));
  }
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,RecordBeforeExit,NULL,
                            &gEfiEventBeforeExitBootServicesGuid,&Event);
}
