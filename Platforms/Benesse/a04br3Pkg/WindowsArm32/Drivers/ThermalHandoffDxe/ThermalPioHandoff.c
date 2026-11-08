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
/* CT3 resume: reset SET/CLR pair, then clock_on. CT3 never polls STA.
 * v3 proves the stock software stop AFTER clocks are live, not from gated
 * zero reads or an invented assertion-latch contract. */
VOID A04ThermalPioBeforeExit(VOID) {
 A04_THERMAL_PIO_RECORD r={0};EFI_GUID guid=A04_THERMAL_PIO_GUID;EFI_STATUS status;UINT32 v,stage1Readback;UINTN n;
 if(mAttempted) { return; }
 mAttempted=TRUE;r.version=3;r.bytes=sizeof(r);r.stage=1;
 r.clock_before=MmioRead32(TOP+0x24);r.reset_before=MmioRead32(I+0x128);
 if(r.clock_before==MAX_UINT32||r.reset_before==MAX_UINT32)goto publish;
 /* Stock resume reset pair. W1S/W1C bit0 only, no unrelated reset bits.
  * DSB matches THERMAL_WRAP_WR32. STA is diagnostic, never a ready premise. */
 ThermalSyncWrite32(I+0x120,1);
 ThermalSyncWrite32(I+0x124,1);
 ThermalSyncWrite32(TOP+0x84,r.clock_before&0x40000002U);
 stage1Readback=MmioRead32(TOP+0x24);
 if(stage1Readback==MAX_UINT32||(stage1Readback&0x40000002U)) {
  DEBUG((DEBUG_ERROR,"A04 THM PIO stage1 phase=2(ungate) actual=%08x\n",stage1Readback));goto publish;
 }
 r.reset_after=MmioRead32(I+0x128);
 r.stage=2;r.irq_before=MmioRead32(T+0xc);r.ahb_before=MmioRead32(T+0x418);
 ThermalSyncWrite32(T+0xc,0);r.irq_after=MmioRead32(T+0xc);if(r.irq_after)goto publish;
 if(!Idle(T+0x418,0xffff0000U,50,2))goto publish;
 r.stage=3;v=MmioRead32(T+0x3c);if(v==MAX_UINT32)goto publish;
 ThermalSyncWrite32(T+0x3c,v|0x10e);r.pause_after=MmioRead32(T+0x3c);if((r.pause_after&0x10e)!=0x10e)goto publish;
 ThermalSyncWrite32(T,0);r.mon_after=MmioRead32(T);if(r.mon_after)goto publish;
 if(!Idle(T+0x418,0xffff0000U,50,2))goto publish;
 r.ahb_after=MmioRead32(T+0x418);r.stage=4;
 // v3 retains pause/MON0 with live clocks, not a reset-held v2 claim.
 // AUXADC Linux suspend clears all software channel triggers. Do not guess
 // other auto-mode configurations: only the known thermal channel11 is removed.
 v=MmioRead16(A);if(v==0xffffU || (v&~0x800U))goto publish;
 ThermalSyncWrite16(A,(UINT16)(v&~0x800U));ThermalSyncWrite16(A+4,0);
 for(n=0;n<32;n++){v=MmioRead16(A+0x10);if(v==0xffffU)goto publish;if(!(v&1))break;gBS->Stall(1000);}if(n==32)goto publish;
 r.stage=5;r.adc_mode=MmioRead16(A);r.adc_trigger=MmioRead16(A+4);r.adc_busy=MmioRead16(A+0x10);
 r.clock_after=MmioRead32(TOP+0x24);r.irq_pending=MmioRead32(T+0x10);
 if(r.adc_mode||r.adc_trigger||(r.adc_busy&1)||r.adc_busy==0xffffU||(r.clock_after&0x40000002U)||r.clock_after==MAX_UINT32||r.irq_pending)goto publish;
 // Re-read the retained stop with clocks live before publishing v3.
 r.pause_after=MmioRead32(T+0x3c);r.mon_after=MmioRead32(T);
 r.ahb_after=MmioRead32(T+0x418);r.irq_after=MmioRead32(T+0xc);
 if(r.pause_after==MAX_UINT32||(r.pause_after&0x10eU)!=0x10eU||r.mon_after||
    r.ahb_after==MAX_UINT32||(r.ahb_after&0xffff0000U)||r.irq_after)goto publish;
 r.stage=6;r.ready=1;
publish:
 status=gRT->SetVariable(A04_THERMAL_PIO_NAME,&guid,6,sizeof(r),&r);
 DEBUG((DEBUG_ERROR,"A04 THM PIO ready=%u stage=%u clock=%08x reset=%08x adc=%x/%x/%x publish=%r\n",r.ready,r.stage,r.clock_after,r.reset_after,r.adc_mode,r.adc_trigger,r.adc_busy,status));
}
