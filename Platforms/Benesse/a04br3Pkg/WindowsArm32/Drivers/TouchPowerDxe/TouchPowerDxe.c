/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * A04BR3 touch VGP1 handoff. Register sequence derived from the board's
 * OsHandoffDxe.c and UniXNU-a04br3 A04BR3Input.cpp.
 */
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>

#include "../../Include/PwrapRead.h"
#define VGP_ENABLE 0x050AU
#define VGP_VOLTAGE 0x0530U

STATIC EFI_STATUS WritePmic(UINT16 Address, UINT16 Value) {
  EFI_STATUS Status=WaitFsm(0);
  if (EFI_ERROR(Status)) return Status;
  MmioWrite32(WACS_CMD, BIT31|(((UINT32)Address>>1)<<16)|Value);
  return WaitFsm(0);
}
STATIC VOID EFIAPI TouchPowerBeforeExit(EFI_EVENT Event, VOID *Context) {
  EFI_STATUS Status=EFI_NOT_READY;
  UINT16 Voltage=0, Enable=0;
  if ((MmioRead32(WACS_DATA)&BIT21)!=0) Status=ReadPmic(VGP_VOLTAGE,&Voltage);
  if (!EFI_ERROR(Status)) Status=WritePmic(VGP_VOLTAGE,(UINT16)((Voltage&~0xe0U)|0x60U));
  if (!EFI_ERROR(Status)) Status=ReadPmic(VGP_ENABLE,&Enable);
  if (!EFI_ERROR(Status)) Status=WritePmic(VGP_ENABLE,(UINT16)(Enable|BIT15));
  if (!EFI_ERROR(Status)) { MicroSecondDelay(20000); Status=ReadPmic(VGP_VOLTAGE,&Voltage); }
  if (!EFI_ERROR(Status)) Status=ReadPmic(VGP_ENABLE,&Enable);
  if (!EFI_ERROR(Status) && ((Voltage&0xe0U)!=0x60U || (Enable&BIT15)==0)) Status=EFI_DEVICE_ERROR;
  DEBUG((DEBUG_ERROR,"A04TOUCH power status=%r voltage=%04x enable=%04x\n",Status,Voltage,Enable));
}
EFI_STATUS EFIAPI TouchPowerEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_EVENT Event;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,TouchPowerBeforeExit,NULL,
                            &gEfiEventBeforeExitBootServicesGuid,&Event);
}
