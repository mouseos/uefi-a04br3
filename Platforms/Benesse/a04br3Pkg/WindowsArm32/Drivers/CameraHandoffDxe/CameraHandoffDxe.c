/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * A04BR3-only, opt-in pinned camera power/clock handoff. No I2C access.
 * Board kernel 7c478212: scpsys/clk/regulator/pinctrl; board Linux3d2c74e
 * camera-clock's identified HI556 path. Shared banks are touched only in
 * firmware before EBS; Windows receives only camera-private resources. */
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/AcpiTable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/A04BR3TestCycleLib.h>
#include "CameraAml.h"
#define TOP 0x10000000U
#define GPIO 0x10005000U
#define SPM 0x10006000U
#define IMG 0x15000000U
#define SEN 0x15008000U
#define MAGIC 0x4134434dU
STATIC volatile UINT32 *mNvs;
STATIC BOOLEAN Wait(UINTN Reg,UINT32 Mask,UINT32 Value)
{
 UINTN I;for(I=0;I<10000;I++){if((MmioRead32(Reg)&Mask)==Value)return TRUE;MicroSecondDelay(10);}return FALSE;
}
STATIC VOID Update(UINTN Reg,UINT32 Mask,UINT32 Value)
{MmioWrite32(Reg,(MmioRead32(Reg)&~Mask)|(Value&Mask));}
STATIC BOOLEAN Pmic(UINT32 Reg,UINT32 Mask,UINT32 Value)
{
 UINT32 Old,Check;
 if(!A04BR3PmicRead(Reg,&Old)||!A04BR3PmicWrite(Reg,(Old&~Mask)|(Value&Mask))||!A04BR3PmicRead(Reg,&Check))return FALSE;
 return (Check&Mask)==(Value&Mask);
}
STATIC BOOLEAN Rail(UINT32 Reg,UINT32 Enable)
{
 UINT32 Value;UINTN I;if(!Pmic(Reg,Enable,Enable))return FALSE;
 for(I=0;I<1000;I++){if(!A04BR3PmicRead(Reg,&Value))return FALSE;if(Value&BIT15)return TRUE;MicroSecondDelay(10);}return FALSE;
}
STATIC EFI_STATUS Configure(VOID)
{
 UINT32 V,ClockCount,SavedMeter;
 /* Do not reprogram a shared PLL or a live rail to a different voltage. */
 if(!(MmioRead32(0x10018140U)&1U)||(MmioRead32(0x10018150U)&3U)!=1U)return EFI_NOT_READY;
 if(!(MmioRead32(SPM+0x60c)&BIT3)||!(MmioRead32(SPM+0x610)&BIT3))return EFI_NOT_READY;
 if(!A04BR3PmicRead(0x055a,&V))return EFI_DEVICE_ERROR;
 if((V&BIT14)&&(!A04BR3PmicRead(0x055c,&V)||(V&0x60)))return EFI_UNSUPPORTED;
 /* GPIO48/49 only. Never change MMA's GPIO52/53 or its I2C clock/timing. */
 MmioWrite32(GPIO+0x398,(7U<<9)|(7U<<12));
 MmioWrite32(GPIO+0x118,BIT16|BIT17);MmioWrite32(GPIO+0x014,BIT16|BIT17);
 /* Kernel ISP domain: MM clock prerequisite, ON/2ND, ACK, CLK/ISO/RST, SRAM. */
 MmioWrite32(TOP+0x080,BIT9);
 /* CCF MM_SMI_COMMON prerequisite, already shared with active display. */
 MmioWrite32(0x14000108U,BIT0);
 Update(SPM+0x238,BIT2,BIT2);Update(SPM+0x238,BIT3,BIT3);
 if(!Wait(SPM+0x60c,BIT5,BIT5)||!Wait(SPM+0x610,BIT5,BIT5))return EFI_TIMEOUT;
 Update(SPM+0x238,BIT4,0);Update(SPM+0x238,BIT1,0);Update(SPM+0x238,BIT0,BIT0);
 Update(SPM+0x238,0xf00,0);if(!Wait(SPM+0x238,0x3000,0))return EFI_TIMEOUT;
 MmioWrite32(TOP+0x080,BIT1);MmioWrite32(TOP+0x050,BIT4);MmioWrite32(IMG+8,0x1e1);
 Update(TOP,7U<<15,2U<<15);
 MmioWrite32(GPIO+0x448,7U<<6);MmioWrite32(GPIO+0x444,1U<<6);
 Update(GPIO+0xd50,0x300,0x200);
 /* The two prerequisites whose omission caused real HI556 NACKs. */
 Update(SEN+0x100,BIT0,BIT0);Update(SEN+0x120,BIT31,BIT31);
 Update(SEN,BIT10|BIT11,BIT10|BIT11);
 Update(SEN+0x200,BIT31,BIT31);
 Update(SEN+0x204,0x003f3f3f,0x00010001);
 Update(SEN+0x200,BIT31|BIT28|BIT6|BIT2|3U,BIT31|1U);
 MicroSecondDelay(100);
 /* Same stock CKGEN measurement; restore the shared meter immediately. */
 SavedMeter=MmioRead32(TOP+0x10);MmioWrite32(TOP+0x10,BIT14);MmioWrite32(TOP+0x10,0);
 MmioWrite32(TOP+0x10,(32U<<16)|BIT15|519U);MicroSecondDelay(30);ClockCount=MmioRead32(TOP+0x14);
 MmioWrite32(TOP+0x10,BIT14);MmioWrite32(TOP+0x10,SavedMeter);
 if(ClockCount!=960U)return EFI_UNSUPPORTED;
 /* Exact board HI556 rail order: IO1.8, A2.8, D1.2. */
 if(!Rail(0x055e,BIT14)||!Rail(0x0408,BIT15)||!Pmic(0x055c,0x60,0)||!Rail(0x055a,BIT14))return EFI_DEVICE_ERROR;
 Update(SEN+0x200,BIT29,BIT29);
 MmioWrite32(GPIO+0x114,BIT16);MicroSecondDelay(1000);MmioWrite32(GPIO+0x114,BIT17);MicroSecondDelay(1000);
 /* IRQ180/183 inversion follows the existing MT8167 sysirq contract. */
 Update(0x10200634U,BIT20|BIT23,BIT20|BIT23);
 if((MmioRead32(GPIO+0x390)&((7U<<9)|(7U<<12)))||
    (MmioRead32(GPIO+0x010)&(BIT16|BIT17))!=(BIT16|BIT17)||
    (MmioRead32(GPIO+0x110)&(BIT16|BIT17))!=(BIT16|BIT17)||
    (MmioRead32(GPIO+0x440)&(7U<<6))!=(1U<<6)||
    (MmioRead32(SEN+0x200)&0xb0000047U)!=0xa0000001U||
    (MmioRead32(SEN+0x204)&0x003f3f3fU)!=0x00010001U||
    (MmioRead32(IMG)&0x1e1U))return EFI_DEVICE_ERROR;
 mNvs[4]=MmioRead32(0x10018144U);mNvs[5]=48000U;mNvs[6]=0x0556U;
 return EFI_SUCCESS;
}
STATIC VOID EFIAPI BeforeExit(EFI_EVENT Event,VOID *Context)
{
 EFI_STATUS Status;(VOID)Event;(VOID)Context;mNvs[0]=0;WriteBackDataCacheRange((VOID *)mNvs,4096);
 Status=Configure();mNvs[3]=(UINT32)Status;
 if(!EFI_ERROR(Status)){mNvs[1]=1;mNvs[2]=~MAGIC;MemoryFence();mNvs[0]=MAGIC;}
 WriteBackDataCacheRange((VOID *)mNvs,4096);DEBUG((DEBUG_ERROR,"A04CAM pinned handoff=%r\n",Status));
}
STATIC BOOLEAN Patch(UINT8 *Data,UINTN Size,UINT32 Pattern,UINT32 Address)
{
 UINTN I,Count=0;for(I=36;I+4<=Size;I++)if(ReadUnaligned32((UINT32 *)(Data+I))==Pattern){WriteUnaligned32((UINT32 *)(Data+I),Address);Count++;}return Count==1;
}
EFI_STATUS EFIAPI CameraHandoffEntry(EFI_HANDLE Image,EFI_SYSTEM_TABLE *System)
{
 EFI_ACPI_TABLE_PROTOCOL *Acpi;EFI_PHYSICAL_ADDRESS Page=MAX_UINT32;EFI_STATUS Status;EFI_EVENT Event;
 UINT8 Aml[sizeof(mCameraAml)],Sum=0;UINTN I,Key;(VOID)Image;(VOID)System;
 Status=gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid,NULL,(VOID **)&Acpi);if(EFI_ERROR(Status))return Status;
 Status=gBS->AllocatePages(AllocateMaxAddress,EfiACPIMemoryNVS,1,&Page);if(EFI_ERROR(Status))return Status;
 mNvs=(volatile UINT32 *)(UINTN)Page;ZeroMem((VOID *)mNvs,4096);WriteBackDataCacheRange((VOID *)mNvs,4096);
 CopyMem(Aml,mCameraAml,sizeof(Aml));
 if(!Patch(Aml,sizeof(Aml),0x12345678U,(UINT32)Page)||!Patch(Aml,sizeof(Aml),0x87654321U,(UINT32)Page)){gBS->FreePages(Page,1);return EFI_COMPROMISED_DATA;}
 Aml[9]=0;for(I=0;I<sizeof(Aml);I++)Sum=(UINT8)(Sum+Aml[I]);Aml[9]=(UINT8)(0-Sum);
 Status=gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,BeforeExit,NULL,&gEfiEventBeforeExitBootServicesGuid,&Event);
 if(EFI_ERROR(Status)){gBS->FreePages(Page,1);return Status;}
 Status=Acpi->InstallAcpiTable(Acpi,Aml,sizeof(Aml),&Key);
 if(EFI_ERROR(Status)){gBS->CloseEvent(Event);gBS->FreePages(Page,1);}return Status;
}
