// SPDX-License-Identifier: MIT
#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/ArmLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include "ThermalPioRecord.h"
#define T 0x1100d000U
#define A 0x11003000U
#define TOP 0x10000000U
#define I 0x10001000U
/* CT3 sync_write.h: volatile register store followed by mb()==dsb(sy).
 * The selected board IoLib only stores; MemoryFence is DMB, not completion.
 * Use the existing board ArmLib DSB. No additional register access/delay. */
STATIC VOID ThermalSyncWrite32(UINTN Address,UINT32 Value) {
 MmioWrite32(Address,Value);ArmDataSynchronizationBarrier();
}
STATIC VOID ThermalSyncWrite16(UINTN Address,UINT16 Value) {
 MmioWrite16(Address,Value);ArmDataSynchronizationBarrier();
}
STATIC BOOLEAN mAttempted;
STATIC BOOLEAN Idle(UINTN Address,UINT32 Mask,UINTN Attempts,UINTN Delay) {
 UINTN n;UINT32 v;
 for(n=0;n<Attempts;n++) {v=MmioRead32(Address);if(v==MAX_UINT32)return FALSE;if(!(v&Mask))return TRUE;gBS->Stall(Delay);}return FALSE;
}
/* CT3 suspend: idle -> pause all sensing -> MON0 -> clock off.
 * Only an already retained stopped program may use the probe clock-on/reset
 * order. Never prepare or wake an unqualified old AHB address program.
 * Nonzero pause bits reject the all-zero gated-register response.
 * All offsets below are already read by the existing stop/ready path. */
STATIC BOOLEAN ThermalStoppedBeforeUngate(VOID) {
 UINT32 pause,mon,ahb,irq,pending;UINT16 mode,trigger,busy;
 pause=MmioRead32(T+0x3c);mon=MmioRead32(T);
 ahb=MmioRead32(T+0x418);irq=MmioRead32(T+0xc);pending=MmioRead32(T+0x10);
 mode=MmioRead16(A);trigger=MmioRead16(A+4);busy=MmioRead16(A+0x10);
 if(pause==MAX_UINT32||(pause&0x10eU)!=0x10eU||mon||ahb==MAX_UINT32||
    (ahb&0xffff0000U)||irq||pending||mode||trigger||busy==0xffffU||(busy&1)) {
  DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 pre-ungate-stop-proof rejected; no clock/reset write\n"));
  return FALSE;
 }
 return TRUE;
}
VOID A04ThermalPioBeforeExit(VOID) {
 A04_THERMAL_PIO_RECORD r={0};EFI_GUID guid=A04_THERMAL_PIO_GUID;EFI_STATUS status;UINT32 v,stage1Readback;UINTN n;
 if(mAttempted) { return; }
 mAttempted=TRUE;r.version=2;r.bytes=sizeof(r);r.stage=1;
 r.clock_before=MmioRead32(TOP+0x24);r.reset_before=MmioRead32(I+0x128);
 if(r.clock_before==MAX_UINT32||r.reset_before==MAX_UINT32)goto publish;
 if((r.clock_before&0x40000002U)||(r.reset_before&1)) {
  if((r.clock_before&0x40000002U)&&!(r.reset_before&1)) {
   /* Linux probe is clock_on -> reset. Only use it if the old program is
    * already paused/disabled/idle. A failed guard performs no control write. */
   if(!ThermalStoppedBeforeUngate())goto publish;
   ThermalSyncWrite32(TOP+0x84,r.clock_before&0x40000002U);
   stage1Readback=MmioRead32(TOP+0x24);
   if(stage1Readback==MAX_UINT32||(stage1Readback&0x40000002U)) {
    DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 phase=2(ungate) actual=%08x\n",stage1Readback));goto publish;
   }
   ThermalSyncWrite32(I+0x120,1);stage1Readback=MmioRead32(I+0x128);
   if(stage1Readback==MAX_UINT32||!(stage1Readback&1)) {
    DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 phase=1(assert) actual=%08x\n",stage1Readback));goto publish;
   }
  } else {
   /* Existing reset-first path when reset is already asserted. */
   ThermalSyncWrite32(I+0x120,1);stage1Readback=MmioRead32(I+0x128);
   if(stage1Readback==MAX_UINT32||!(stage1Readback&1)) {
    DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 phase=1(assert) actual=%08x\n",stage1Readback));goto publish;
   }
   ThermalSyncWrite32(TOP+0x84,r.clock_before&0x40000002U);
   stage1Readback=MmioRead32(TOP+0x24);
   if(stage1Readback==MAX_UINT32||(stage1Readback&0x40000002U)) {
    DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 phase=2(ungate) actual=%08x\n",stage1Readback));goto publish;
   }
  }
  ThermalSyncWrite32(I+0x124,1);stage1Readback=MmioRead32(I+0x128);
  if(stage1Readback&1) {
   DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 phase=3(deassert) actual=%08x\n",stage1Readback));goto publish;
  }
 }
 r.stage=2;r.irq_before=MmioRead32(T+0xc);r.ahb_before=MmioRead32(T+0x418);
 ThermalSyncWrite32(T+0xc,0);r.irq_after=MmioRead32(T+0xc);if(r.irq_after)goto publish;
 if(!Idle(T+0x418,0xffff0000U,50,2))goto publish;
 r.stage=3;v=MmioRead32(T+0x3c);if(v==MAX_UINT32)goto publish;
 ThermalSyncWrite32(T+0x3c,v|0x10e);r.pause_after=MmioRead32(T+0x3c);if((r.pause_after&0x10e)!=0x10e)goto publish;
 ThermalSyncWrite32(T,0);r.mon_after=MmioRead32(T);if(r.mon_after)goto publish;
 if(!Idle(T+0x418,0xffff0000U,50,2))goto publish;
 r.ahb_after=MmioRead32(T+0x418);r.stage=4;
 // No future thermal AHB/IRQ producer survives this handoff. Linux INFRA reset.
 ThermalSyncWrite32(I+0x120,1);r.reset_after=MmioRead32(I+0x128);if(r.reset_after==MAX_UINT32||!(r.reset_after&1))goto publish;
 // AUXADC Linux suspend clears all software channel triggers. Do not guess
 // other auto-mode configurations: only the known thermal channel11 is removed.
 v=MmioRead16(A);if(v==0xffffU || (v&~0x800U))goto publish;
 ThermalSyncWrite16(A,(UINT16)(v&~0x800U));ThermalSyncWrite16(A+4,0);
 for(n=0;n<32;n++){v=MmioRead16(A+0x10);if(v==0xffffU)goto publish;if(!(v&1))break;gBS->Stall(1000);}if(n==32)goto publish;
 r.stage=5;r.adc_mode=MmioRead16(A);r.adc_trigger=MmioRead16(A+4);r.adc_busy=MmioRead16(A+0x10);
 r.clock_after=MmioRead32(TOP+0x24);r.irq_pending=MmioRead32(T+0x10);
 if(r.adc_mode||r.adc_trigger||(r.adc_busy&1)||r.adc_busy==0xffffU||(r.clock_after&0x40000002U)||r.clock_after==MAX_UINT32||r.irq_pending)goto publish;
 r.stage=6;r.ready=1;
publish:
 status=gRT->SetVariable(A04_THERMAL_PIO_NAME,&guid,6,sizeof(r),&r);
 DEBUG((DEBUG_ERROR,"A04 THM PIO ready=%u stage=%u clock=%08x reset=%08x adc=%x/%x/%x publish=%r\n",r.ready,r.stage,r.clock_after,r.reset_after,r.adc_mode,r.adc_trigger,r.adc_busy,status));
}
