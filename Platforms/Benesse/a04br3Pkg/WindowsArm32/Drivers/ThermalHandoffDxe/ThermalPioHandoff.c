// SPDX-License-Identifier: MIT
#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include "ThermalPioRecord.h"
#define T 0x1100d000U
#define A 0x11003000U
#define TOP 0x10000000U
#define I 0x10001000U
STATIC BOOLEAN mAttempted;
STATIC BOOLEAN Idle(UINTN Address,UINT32 Mask,UINTN Attempts,UINTN Delay) {
 UINTN n;UINT32 v;
 for(n=0;n<Attempts;n++) {v=MmioRead32(Address);if(v==MAX_UINT32)return FALSE;if(!(v&Mask))return TRUE;gBS->Stall(Delay);}return FALSE;
}
VOID A04ThermalPioBeforeExit(VOID) {
 A04_THERMAL_PIO_RECORD r={0};EFI_GUID guid=A04_THERMAL_PIO_GUID;EFI_STATUS status;UINT32 v;UINTN n;
 if(mAttempted)return;mAttempted=TRUE;r.version=2;r.bytes=sizeof(r);r.stage=1;
 r.clock_before=MmioRead32(TOP+0x24);r.reset_before=MmioRead32(I+0x128);
 if(r.clock_before==MAX_UINT32||r.reset_before==MAX_UINT32)goto publish;
 if(r.clock_before&0x40000002U) {
  // Linux reset precedes ungating: never resume an old AHB address program.
  MmioWrite32(I+0x120,1);if(!(MmioRead32(I+0x128)&1))goto publish;
  MmioWrite32(TOP+0x84,r.clock_before&0x40000002U);
  if(MmioRead32(TOP+0x24)&0x40000002U)goto publish;
  MmioWrite32(I+0x124,1);if(MmioRead32(I+0x128)&1)goto publish;
 }
 r.stage=2;r.irq_before=MmioRead32(T+0xc);r.ahb_before=MmioRead32(T+0x418);
 MmioWrite32(T+0xc,0);r.irq_after=MmioRead32(T+0xc);if(r.irq_after)goto publish;
 if(!Idle(T+0x418,0xffff0000U,50,2))goto publish;
 r.stage=3;v=MmioRead32(T+0x3c);if(v==MAX_UINT32)goto publish;
 MmioWrite32(T+0x3c,v|0x10e);r.pause_after=MmioRead32(T+0x3c);if((r.pause_after&0x10e)!=0x10e)goto publish;
 MmioWrite32(T,0);r.mon_after=MmioRead32(T);if(r.mon_after)goto publish;
 if(!Idle(T+0x418,0xffff0000U,50,2))goto publish;
 r.ahb_after=MmioRead32(T+0x418);r.stage=4;
 // No future thermal AHB/IRQ producer survives this handoff. Linux INFRA reset.
 MmioWrite32(I+0x120,1);r.reset_after=MmioRead32(I+0x128);if(r.reset_after==MAX_UINT32||!(r.reset_after&1))goto publish;
 // AUXADC Linux suspend clears all software channel triggers. Do not guess
 // other auto-mode configurations: only the known thermal channel11 is removed.
 v=MmioRead16(A);if(v==0xffffU || (v&~0x800U))goto publish;
 MmioWrite16(A,(UINT16)(v&~0x800U));MmioWrite16(A+4,0);
 for(n=0;n<32;n++){v=MmioRead16(A+0x10);if(v==0xffffU)goto publish;if(!(v&1))break;gBS->Stall(1000);}if(n==32)goto publish;
 r.stage=5;r.adc_mode=MmioRead16(A);r.adc_trigger=MmioRead16(A+4);r.adc_busy=MmioRead16(A+0x10);
 r.clock_after=MmioRead32(TOP+0x24);r.irq_pending=MmioRead32(T+0x10);
 if(r.adc_mode||r.adc_trigger||(r.adc_busy&1)||r.adc_busy==0xffffU||(r.clock_after&0x40000002U)||r.clock_after==MAX_UINT32||r.irq_pending)goto publish;
 r.stage=6;r.ready=1;
publish:
 status=gRT->SetVariable(A04_THERMAL_PIO_NAME,&guid,6,sizeof(r),&r);
 DEBUG((DEBUG_ERROR,"A04 THM PIO ready=%u stage=%u clock=%08x reset=%08x adc=%x/%x/%x publish=%r\n",r.ready,r.stage,r.clock_after,r.reset_after,r.adc_mode,r.adc_trigger,r.adc_busy,status));
}
