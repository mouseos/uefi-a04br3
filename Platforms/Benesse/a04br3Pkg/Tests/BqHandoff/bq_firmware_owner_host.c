/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <Library/A04BR3BqFirmwareOwnerLib.h>
#define NS 1000000000ULL
struct fake { uint64_t ns,tick,step_ns,delay_ns; uint8_t regs[8]; int fail_reg, fail_write; unsigned reads,writes,counters,clocks,regress_at; };
static uint64_t now(void *p) { struct fake *f=p; uint64_t t=f->ns; f->ns+=f->step_ns; if (++f->clocks==f->regress_at) t-=2*f->step_ns; return t; }
static uint64_t physical(void *p) { struct fake *f=p; ++f->counters; return f->tick; }
static int rd(void *p,uint8_t reg,uint8_t *out) { struct fake *f=p; ++f->reads; if(f->fail_reg==(int)reg)return -1; *out=f->regs[reg]; return 0; }
static int wr(void *p,uint8_t value) { struct fake *f=p; assert(value==0x80); ++f->writes; f->tick+=1300000; f->ns+=f->delay_ns; return f->fail_write; }
static void init(struct a04_fw_owner *o,struct fake *f,struct a04_fw_io *io) {
 const uint8_t regs[8]={0x30,0,0x8c,0xa4,0x45,0x51,0,0x38};
 memset(o,0,sizeof(*o));memset(f,0,sizeof(*f)); memcpy(f->regs,regs,8);f->fail_reg=-1;f->tick=130000000;
 io->context=f;io->now_ns=now;io->physical_counter=physical;io->read=rd;io->write_watchdog=wr;
}
int main(void) {
 struct a04_fw_owner fwo; struct a04_bq_owner os; struct fake f; struct a04_fw_io io; uint8_t record[128],saved[128]; unsigned i;
 for(i=0;i<4;i++) { init(&fwo,&f,&io);
   assert(a04_fw_start(&fwo,&io,13000000,20*NS,i!=0,i!=1,i!=2,i!=3));
   assert(a04_fw_tick(&fwo)==A04_BQ_STOPPED && !f.reads && !f.writes); }
 init(&fwo,&f,&io); assert(a04_fw_start(&fwo,&io,13000000,20*NS,1,1,0,1));
 assert(a04_fw_tick(&fwo)==A04_BQ_STOPPED && !f.reads && !f.writes);
 assert(a04_fw_exit(&fwo,143000000,record,128)); memset(&os,0,sizeof(os));
 assert(a04_bq_adopt_handoff(&os,record,128,NS,143000000,13000000));
 init(&fwo,&f,&io); assert(!a04_fw_start(&fwo,&io,13000000,20*NS,1,1,1,1));
 assert(a04_fw_tick(&fwo)==A04_BQ_OK && f.reads==8 && f.writes==1 && fwo.last_tick==130000000);
 assert(!a04_fw_exit(&fwo,143000000,record,128)); memcpy(saved,record,128);
 memset(&os,0,sizeof(os)); assert(!a04_bq_adopt_handoff(&os,record,128,100*NS,143000000,13000000));
 assert(os.handoff_deadline==119*NS && os.firmware_first.valid==255);
 assert(a04_fw_tick(&fwo)==A04_BQ_STOPPED && a04_fw_exit(&fwo,143000000,record,128));
 assert(!memcmp(saved,record,128) && f.reads==8 && f.writes==1 && f.counters==1);
 init(&fwo,&f,&io); assert(!a04_fw_start(&fwo,&io,13000000,NS,1,1,1,1)); f.ns=NS;
 assert(a04_fw_tick(&fwo)==A04_BQ_DEADLINE && !f.reads && !f.writes);
 assert(a04_fw_exit(&fwo,143000000,record,128)); memset(&os,0,sizeof(os));
 assert(a04_bq_adopt_handoff(&os,record,128,NS,143000000,13000000));
 for(i=0;i<8;++i) { init(&fwo,&f,&io);f.fail_reg=(int)i;
   assert(!a04_fw_start(&fwo,&io,13000000,NS,1,1,1,1));assert(a04_fw_tick(&fwo)==A04_BQ_READ_ERROR);
   assert(fwo.bq.first.valid==((1u<<i)-1u));assert(a04_fw_exit(&fwo,143000000,record,128));
   memset(&os,0,sizeof(os));assert(a04_bq_adopt_handoff(&os,record,128,NS,143000000,13000000)); }
 init(&fwo,&f,&io);assert(!a04_fw_start(&fwo,&io,13000000,NS,1,1,1,1));
 assert(a04_fw_exit(&fwo,143000000,0,0)); assert(fwo.exited && !fwo.active);
 assert(a04_fw_tick(&fwo)==A04_BQ_STOPPED && !f.reads && !f.writes);
 init(&fwo,&f,&io);assert(!a04_fw_start(&fwo,&io,13000000,20*NS,1,1,1,1));
 assert(a04_fw_tick(&fwo)==A04_BQ_OK); f.tick=1; f.ns=NS;
 assert(a04_fw_tick(&fwo)==A04_BQ_WRITE_ERROR && f.writes==1);
 assert(a04_fw_exit(&fwo,143000000,record,128));
 init(&fwo,&f,&io);assert(!a04_fw_start(&fwo,&io,13000000,NS,1,1,1,1));f.fail_write=1;
 assert(a04_fw_tick(&fwo)==A04_BQ_WRITE_ERROR && fwo.last_tick==0);
 assert(a04_fw_exit(&fwo,143000000,record,128));
 init(&fwo,&f,&io);assert(!a04_fw_start(&fwo,&io,13000000,NS,1,1,1,1));
 assert(a04_fw_tick(&fwo)==A04_BQ_OK);assert(a04_fw_exit(&fwo,390000000,record,128));
 memset(&os,0,sizeof(os));assert(a04_bq_adopt_handoff(&os,record,128,NS,390000000,13000000));
 init(&fwo,&f,&io);f.ns=NS;f.delay_ns=900000000;
 assert(!a04_fw_start(&fwo,&io,13000000,30*NS,1,1,1,1));
 assert(a04_fw_tick(&fwo)==A04_BQ_OK && fwo.bq.last_kick==NS);
 f.ns=21*NS;assert(a04_fw_tick(&fwo)==A04_BQ_DEADLINE && f.reads==8 && f.writes==1);
 init(&fwo,&f,&io);f.ns=100;f.step_ns=10;f.regress_at=4;
 assert(!a04_fw_start(&fwo,&io,13000000,NS,1,1,1,1));
 assert(a04_fw_tick(&fwo)==A04_BQ_DEADLINE && f.reads==2 && !f.writes);
 return 0;
}
