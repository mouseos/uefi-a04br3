/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <Library/A04BR3BqFirmwareOwnerLib.h>
#include <Library/A04BR3BqPio.h>
struct fake { uint8_t chip[8], tx[2]; unsigned tx_count, transfer, fail_at, writes; uint16_t regs[128]; uint64_t ns,tick; };
static uint16_t rd(void *p,unsigned r){struct fake *f=p;if(r==0x24)return 0;if(r==0x0c)return f->transfer==f->fail_at?2:1;if(r==0x30)return 0x10;if(r==0)return f->chip[f->tx[0]];return f->regs[r/2];}
static void wr(void *p,unsigned r,uint16_t v){struct fake *f=p;f->regs[r/2]=v;if(r==0x38)f->tx_count=0;if(!r&&f->tx_count<2)f->tx[f->tx_count++]=(uint8_t)v;if(r==0x24){++f->transfer;if(f->tx_count==2){assert(f->tx[0]==0&&f->tx[1]==0x80);++f->writes;}}}
static uint64_t now(void *p){return ((struct fake *)p)->ns;}
static uint64_t physical(void *p){return ((struct fake *)p)->tick;}
static void delay(void *p,unsigned us){((struct fake *)p)->ns+=(uint64_t)us*1000;}
struct context { struct fake fake; struct a04_bq_pio pio; };
static uint64_t owner_now(void *p){return now(&((struct context *)p)->fake);}
static uint64_t owner_counter(void *p){return physical(&((struct context *)p)->fake);}
static int owner_read(void *p,uint8_t reg,uint8_t *v){return a04_bq_pio_read(&((struct context *)p)->pio,reg,v);}
static int owner_kick(void *p,uint8_t v){return a04_bq_pio_kick(&((struct context *)p)->pio,v);}
static void init(struct context *c){const uint8_t image[8]={0x30,0,0x8c,0xa4,0x45,0x51,0,0x38};memset(c,0,sizeof(*c));memcpy(c->fake.chip,image,8);c->fake.tick=130000000;c->pio.context=&c->fake;c->pio.read16=rd;c->pio.write16=wr;c->pio.now_ns=now;c->pio.delay_us=delay;}
int main(void){struct context c;struct a04_fw_owner owner={0};struct a04_bq_owner win={0};uint8_t record[128];struct a04_fw_io io={&c,owner_now,owner_counter,owner_read,owner_kick};
 init(&c);assert(!a04_fw_start(&owner,&io,13000000,20000000000ULL,1,1,1,1));assert(a04_fw_tick(&owner)==A04_BQ_OK&&c.fake.transfer==9&&c.fake.writes==1);
 assert(!a04_fw_exit(&owner,143000000,record,128));assert(!a04_bq_adopt_handoff(&win,record,128,0,143000000,13000000));
 init(&c);memset(&owner,0,sizeof(owner));c.fake.fail_at=3;assert(!a04_fw_start(&owner,&io,13000000,20000000000ULL,1,1,1,1));
 assert(a04_fw_tick(&owner)==A04_BQ_READ_ERROR&&c.pio.faulted&&c.fake.transfer==3&&!c.fake.writes);assert(a04_fw_tick(&owner)==A04_BQ_STOPPED);
 assert(a04_fw_exit(&owner,143000000,record,128));memset(&win,0,sizeof(win));assert(a04_bq_adopt_handoff(&win,record,128,0,143000000,13000000));return 0;}
