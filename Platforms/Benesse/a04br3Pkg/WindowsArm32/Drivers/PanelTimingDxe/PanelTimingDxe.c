/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Guid/EventGroup.h>
#include <A04BR3PanelTiming.h>
#include "PanelTimingAml.h"
STATIC EFI_GUID mPanelGuid=A04BR3_PANEL_TIMING_GUID;
STATIC BOOLEAN HasDisplayDevice(EFI_ACPI_SDT_PROTOCOL *Sdt,UINTN Key) {
 /* ACPI encoded absolute dual NameSeg path: \_SB_.DIS0. */
 UINT8 Path[]={0x5c,0x2e,'_','S','B','_','D','I','S','0',0};
 EFI_ACPI_HANDLE Root=NULL,Device=NULL;EFI_ACPI_DATA_TYPE Type;
 CONST VOID *Data=NULL;UINTN Bytes=0;EFI_STATUS Status;BOOLEAN Found=FALSE;
 Status=Sdt->OpenSdt(Key,&Root);if(EFI_ERROR(Status)) return FALSE;
 Status=Sdt->FindPath(Root,Path,&Device);
 if(!EFI_ERROR(Status)&&Device) {
  Status=Sdt->GetOption(Device,0,&Type,&Data,&Bytes);
  Found=!EFI_ERROR(Status)&&Type==EFI_ACPI_DATA_TYPE_OPCODE&&Bytes==2&&
        Data&&((CONST UINT8 *)Data)[0]==0x5b&&((CONST UINT8 *)Data)[1]==0x82;
 }
 if(Device&&Device!=Root) Sdt->Close(Device);Sdt->Close(Root);return Found;
}
STATIC VOID EFIAPI Publish(EFI_EVENT Event,VOID *Context) {
 EFI_ACPI_TABLE_PROTOCOL *Acpi;EFI_ACPI_SDT_PROTOCOL *Sdt;
 EFI_ACPI_SDT_HEADER *Header;EFI_ACPI_TABLE_VERSION Version;
 EFI_STATUS Status;VOID *Hob;A04BR3_PANEL_TIMING Record;UINTN I,Key;
 UINT8 Table[sizeof(mPanelTimingSsdt)],Sum=0;
 (VOID)Context;gBS->CloseEvent(Event);
 Status=gBS->LocateProtocol(&gEfiAcpiSdtProtocolGuid,NULL,(VOID **)&Sdt);if(EFI_ERROR(Status)) return;
 for(I=0;I<256;I++) {
  Status=Sdt->GetAcpiTable(I,&Header,&Version,&Key);if(EFI_ERROR(Status)) return;
  if(Header&&Header->Length>=sizeof(*Header)&&Header->Signature==SIGNATURE_32('S','S','D','T')&&CompareMem(&Header->OemTableId,"A04DISP ",8)==0&&HasDisplayDevice(Sdt,Key)) break;
 }
 if(I==256) return;
 ZeroMem(&Record,sizeof(Record));Record.Magic=A04BR3_PANEL_TIMING_MAGIC;Record.Version=1;Record.Bytes=sizeof(Record);
 Hob=GetFirstGuidHob(&mPanelGuid);
 if(Hob&&GET_GUID_HOB_DATA_SIZE(Hob)==sizeof(Record)) CopyMem(&Record,GET_GUID_HOB_DATA(Hob),sizeof(Record));
 if(Record.Magic!=A04BR3_PANEL_TIMING_MAGIC||Record.Version!=1||Record.Bytes!=64||Record.Flags!=1) {
  DEBUG((DEBUG_ERROR,"A04 PANEL: LK timing absent; no valid DTM0 published\n"));return;
 }
 CopyMem(Table,mPanelTimingSsdt,sizeof(Table));CopyMem(Table+A04_PANEL_AML_RECORD_OFFSET,&Record,sizeof(Record));
 Table[9]=0;for(I=0;I<sizeof(Table);I++) Sum=(UINT8)(Sum+Table[I]);Table[9]=(UINT8)(0-Sum);
 Status=gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid,NULL,(VOID **)&Acpi);
 if(!EFI_ERROR(Status)) Status=Acpi->InstallAcpiTable(Acpi,Table,sizeof(Table),&Key);
 DEBUG((DEBUG_ERROR,"A04 PANEL: DTM0 fps100=%u publish=%r\n",Record.Fps100,Status));
}
EFI_STATUS EFIAPI PanelTimingEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *System) {
 EFI_EVENT Event;(VOID)Image;(VOID)System;
 return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,Publish,NULL,&gEfiEventReadyToBootGuid,&Event);
}
