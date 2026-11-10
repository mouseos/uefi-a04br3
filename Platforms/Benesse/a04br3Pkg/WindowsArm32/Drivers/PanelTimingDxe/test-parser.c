/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
typedef uint8_t UINT8;typedef uint32_t UINT32;typedef size_t UINTN;typedef char CHAR8;typedef int BOOLEAN;
#define STATIC static
#define CONST const
#define TRUE 1
#define FALSE 0
#define A04BR3_PANEL_TIMING_PARSER 1
#include <A04BR3PanelTiming.h>
static void put(UINT8 *p,UINT32 v){for(unsigned i=0;i<4;i++)p[i]=(UINT8)(v>>(i*8));}
int main(void){
 UINT8 b[48]={0};A04BR3_PANEL_TIMING r;
 _Static_assert(sizeof(r)==64,"DTM0 wire bytes");
 put(b,0xbe9c0000);put(b+8,1);put(b+12,5990);put(b+16,0x1000000);memcpy(b+20,"auo_wxga_dsi",13);
 assert(A04PanelParse(b,sizeof(b),&r));assert(r.Fps100==5990&&r.FpsDenominator==100&&r.TotalWidth==960&&r.TotalHeight==1300);
 put(b+12,6008);assert(A04PanelParse(b,sizeof(b),&r)&&r.Fps100==6008);
 assert(!A04PanelParse(b,32,&r)&&r.Flags==0);assert(!A04PanelParse(NULL,48,&r)&&r.Flags==0);
 b[20]='x';assert(!A04PanelParse(b,sizeof(b),&r)&&!r.Flags);b[20]='a';
 put(b+12,0);assert(!A04PanelParse(b,sizeof(b),&r)&&!r.Flags);
 put(b+12,6000);put(b+4,1);assert(!A04PanelParse(b,sizeof(b),&r));put(b+4,0);
 put(b+8,0);assert(!A04PanelParse(b,sizeof(b),&r));put(b+8,1);
 put(b+16,4095);assert(!A04PanelParse(b,sizeof(b),&r));return 0;
}
