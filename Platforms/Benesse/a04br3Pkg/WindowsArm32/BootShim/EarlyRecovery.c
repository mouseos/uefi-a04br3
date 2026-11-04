/* Pre-DXE RTC fastboot recovery, derived from board A04BR3TestCycleLib.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#ifndef A04_RECOVERY_MMIO
static uint32_t rd(uintptr_t a) { return *(volatile uint32_t *)a; }
static void wr(uintptr_t a,uint32_t v) { *(volatile uint32_t *)a=v; __asm__ volatile("dsb sy" ::: "memory"); }
#else
extern uint32_t rd(uintptr_t);
extern void wr(uintptr_t,uint32_t);
#endif
#define RDATA 0x1000f0a4U
#define CMD 0x1000f0a0U
#define VLD 0x1000f0a8U
static int wacs(int write,uint32_t addr,uint32_t data,uint32_t *out) {
  uint32_t s,i;
  for(i=0;i<1000000;i++) { s=rd(RDATA); if(!(s&(1U<<21))) return 0;
    if(((s>>16)&7)==0) break; if(((s>>16)&7)==6) wr(VLD,1); }
  if(i==1000000) return 0;
  wr(CMD,(write?1U<<31:0)|((addr>>1)<<16)|(data&0xffff));
  if(write) return 1;
  for(i=0;i<1000000;i++) { s=rd(RDATA); if(!(s&(1U<<21))) return 0;
    if(((s>>16)&7)==6) { *out=s&0xffff; wr(VLD,1); return 1; } }
  return 0;
}
static int trigger(void) {
  uint32_t x,i; if(!wacs(1,0x803c,1,0)) return 0;
  for(i=0;i<100000;i++) { if(!wacs(0,0x8000,0,&x)) return 0; if(!(x&64)) return 1; }
  return 0;
}
/* Return success only after the fastboot flag reads back set. */
int A04PrepareFastbootRecovery(void) {
  uint32_t p;
  if(!wacs(1,0x8036,0x586a,0)||!trigger()||!wacs(1,0x8036,0x9136,0)||!trigger()||
     !wacs(0,0x802c,0,&p)||!wacs(1,0x802c,p|0x2000,0)||!trigger()||
     !wacs(0,0x802c,0,&p)||!(p&0x2000)) return 0;
  wr(0x10007004,((31U<<6)<<5)|8U);
  wr(0x10007008,0x1971);
  p=rd(0x10007000);
  wr(0x10007000,(p&0x00ffffffU&~0x48U)|0x22000015U);
  return (rd(0x10007000)&1U)!=0;
}
