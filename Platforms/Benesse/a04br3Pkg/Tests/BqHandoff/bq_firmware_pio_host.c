/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <Library/A04BR3BqPio.h>
struct fake { uint16_t regs[128], data[4]; unsigned delay,count; int mode; };
static uint16_t rd(void *p,unsigned r) {struct fake *f=p;if(r==0x24)return f->mode==4;if(r==0x0c)return f->mode==1?2:f->mode==2?0:1;if(r==0x30)return f->mode==3?0x20:0x10;if(r==0)return 0xa5;return f->regs[r/2];}
static void wr(void *p,unsigned r,uint16_t v){struct fake *f=p;f->regs[r/2]=v;if(!r&&f->count<4)f->data[f->count++]=v;}
static void delay(void *p,unsigned us){struct fake *f=p;assert(us==5);++f->delay;}
static uint64_t now(void *p){return (uint64_t)((struct fake *)p)->delay*5000;}
int main(void){struct fake f;struct a04_bq_pio p={&f,rd,wr,now,delay,0};uint8_t v=0;int mode;
 memset(&f,0,sizeof(f));assert(!a04_bq_pio_read(&p,4,&v)&&v==0xa5);assert(f.regs[2]==0xd6&&f.data[0]==4);
 memset(&f,0,sizeof(f));assert(!a04_bq_pio_kick(&p,0x80)&&f.data[0]==0&&f.data[1]==0x80);
 memset(&f,0,sizeof(f));assert(a04_bq_pio_kick(&p,0x81)&&!f.count);
 for(mode=1;mode<=4;mode++){memset(&f,0,sizeof(f));f.mode=mode;p.faulted=0;v=0x55;
  assert(a04_bq_pio_read(&p,0,&v)&&p.faulted&&v==0x55);f.mode=0;assert(a04_bq_pio_kick(&p,0x80));}
 return 0;}
