// SPDX-License-Identifier: MIT
// Board-only observation. No GCE, IRQ, event, or clock-gate write occurs.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include "GceObservationRecord.h"
STATIC EFI_GUID mRecordGuid={0xb0f4cc26,0x0ad6,0x48e9,{0x94,0x74,0xe0,0x37,0x2a,0xcf,0x32,0x41}};
STATIC CONST CHAR16 mName[]=L"A04BR3GceObservation";
STATIC VOID EFIAPI ObserveBeforeExit(EFI_EVENT Event,VOID *Context)
{
  A04_GCE_OBSERVATION_RECORD Record={1,sizeof(Record),A04_GCE_OBSERVATION_TOP2,0,
                                    UINT32_MAX,UINT32_MAX,UINT32_MAX,0};
  EFI_STATUS Status;
  (VOID)Event;(VOID)Context;
  // TOPCKGEN is already mapped and read by DisplayInitDxe. The TOP2 gate
  // alone cannot prove that a nonsecure GCE access is safe: do not read it.
  Record.top2_status=MmioRead32(0x1000003cu);
  Status=gRT->SetVariable((CHAR16 *)mName,&mRecordGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,sizeof(Record),&Record);
  DEBUG((DEBUG_ERROR,"A04 GCE: top2=%08x record=%r thread/irq/secure ownership unknown\n",Record.top2_status,Status));
}
EFI_STATUS EFIAPI GceObservationEntry(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;EFI_STATUS Status;
  (VOID)ImageHandle;(VOID)SystemTable;
  // A previous boot's record must never be confused with this observation.
  Status=gRT->SetVariable((CHAR16 *)mName,&mRecordGuid,0,0,NULL);
  if(EFI_ERROR(Status) && Status!=EFI_NOT_FOUND)return Status;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,ObserveBeforeExit,NULL,
                            &gEfiEventBeforeExitBootServicesGuid,&Event);
}
