/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define A04_I2C1_HOST_TEST 1
#include "../../Drivers/I2c1HandoffDxe/I2c1HandoffDxe.c"
static UINT32 top[1024], infra[1024], gpio[1024], apmixed[1024];
static UINT16 i2c[2048];
static int inaccessible, accessed_while_gated, fail_gate;
struct mock_bs *gBS; int gEfiEventBeforeExitBootServicesGuid;
static UINT32 *loc(UINT32 a) {
 if (a>=TOP && a<TOP+0x1000) return &top[(a-TOP)/4];
 if (a>=INFRA && a<INFRA+0x1000) return &infra[(a-INFRA)/4];
 if (a>=GPIO && a<GPIO+0x1000) return &gpio[(a-GPIO)/4];
 assert(a>=APMIXED && a<APMIXED+0x1000); return &apmixed[(a-APMIXED)/4];
}
UINT32 MmioRead32(UINT32 a) { return *loc(a); }
UINT16 MmioRead16(UINT32 a) { if (top[0x24/4]&BIT4) accessed_while_gated=1; return i2c[(a-I2C)/2]; }
void MmioWrite16(UINT32 a,UINT16 v) { if (top[0x24/4]&BIT4) accessed_while_gated=1; i2c[(a-I2C)/2]=v; }
void MmioWrite32(UINT32 a,UINT32 v) {
 if (a==TOP+0x84) { if (!fail_gate) top[0x24/4]&=~v; return; }
 if (a==GPIO+0x3a8) { gpio[0x3a0/4]&=~v; return; }
 if (a==GPIO+0x3a4) { gpio[0x3a0/4]|=v; return; }
 if (a==GPIO+0x914) { gpio[0x910/4]|=v; return; }
 if (a==GPIO+0x538) { gpio[0x530/4]&=~v; return; }
 *loc(a)=v;
}
int main(void) {
 UINT32 timing, sample, step;
 top[0]=1u<<4; top[0x24/4]=BIT4; infra[0x80/4]=0xff;
 gpio[0x3a0/4]=0xffffffff; gpio[0x530/4]=0xffffffff;
 assert(ConfigureI2c1()==EFI_SUCCESS && !accessed_while_gated);
 timing=i2c[0x20/2]; sample=((timing>>8)&7)+1; step=(timing&63)+1;
 assert(13000000u <= 200000u*sample*step);
 assert((infra[0x80/4]&BIT2)==0 && (infra[0x80/4]&0xfbu)==0xfbu);
 assert((gpio[0x3a0/4]&0xfc0u)==0x240u);
 assert((gpio[0x530/4]&0x30u)==0 && (gpio[0x530/4]&0xffffffcfu)==0xffffffcfu);
 top[0x24/4]=BIT4; fail_gate=1; accessed_while_gated=0;
 assert(ConfigureI2c1()==EFI_DEVICE_ERROR && !accessed_while_gated);
 top[0]=0; fail_gate=0; inaccessible=0;
 assert(ConfigureI2c1()==EFI_UNSUPPORTED && !inaccessible);
 puts("I2C1 handoff mocked MMIO tests passed"); return 0;
}
