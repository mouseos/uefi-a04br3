/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read-only, one ReadyToBoot check through the actual standard table after
 * RTC Arch installation. Never a runtime log or PMIC transaction. */
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
STATIC VOID EFIAPI MtRtcBootProbe(IN EFI_EVENT Event,IN VOID *Context){
 EFI_STATUS status;EFI_TIME time;EFI_TIME_CAPABILITIES capabilities;
 (VOID)Context;gBS->CloseEvent(Event);
 if(!gRT||!gRT->GetTime){DEBUG((DEBUG_ERROR,"MT6392 UEFI table GetTime unavailable\n"));return;}
 status=gRT->GetTime(&time,NULL);
 DEBUG((DEBUG_ERROR,"MT6392 UEFI table GetTime time-only status=%r\n",status));
 if(!EFI_ERROR(status))
  DEBUG((DEBUG_ERROR,"MT6392 UEFI table local %04u-%02u-%02u %02u:%02u:%02u timezone=%d; OS sync unverified\n",time.Year,time.Month,time.Day,time.Hour,time.Minute,time.Second,time.TimeZone));
 status=gRT->GetTime(&time,&capabilities);
 /* No capability fields are printed when unsupported. Never treat zeroed
  * or unspecified capability output as measured oscillator accuracy. */
 DEBUG((DEBUG_ERROR,"MT6392 UEFI table GetTime capabilities status=%r\n",status));
}
VOID MtRtcRegisterBootProbe(VOID){
 EFI_EVENT event;EFI_STATUS status;
 status=gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,MtRtcBootProbe,NULL,&gEfiEventReadyToBootGuid,&event);
 DEBUG((DEBUG_ERROR,"MT6392 UEFI GetTime boot probe registration=%r\n",status));
 /* Missing diagnostics must not withhold the RTC Arch protocol/BDS. No
  * event/registration pointer is stored in the runtime clock state. */
}
