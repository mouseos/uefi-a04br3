/* SPDX-License-Identifier: MIT
 * The wire format follows UniXNU-a04br3 A04BR3ChargerHandoff.h (MIT).
 * No direct firmware service, MMIO, clock, PMIC or I2C access is implemented.
 */
#include <Library/A04BR3BqFirmwareOwnerLib.h>
#define MAX_GAP 20000000000ULL
static uint64_t fw_now(void *p)
{ struct a04_fw_owner *f=p; return f->io.now_ns(f->io.context); }
static int fw_read(void *p, uint8_t reg, uint8_t *v)
{ struct a04_fw_owner *f=p; return f->io.read(f->io.context,reg,v); }
static int fw_kick(void *p, uint8_t value)
{
    struct a04_fw_owner *f=p;
    /* Sampling before the write is conservative even if the transfer stalls. */
    uint64_t tick=f->io.physical_counter(f->io.context);
    if (f->bq.kicks && tick < f->last_tick) return -1;
    if (f->io.write_watchdog(f->io.context,value)) return -1;
    f->last_tick=tick;
    return 0;
}
int a04_fw_start(struct a04_fw_owner *f, const struct a04_fw_io *io, uint64_t freq,
                 uint64_t deadline, int timer, int exclusive, int receiver, int continuity)
{
    if (!f) return -1;
    if (f->active || f->exited || f->bq.stopped || f->bq.attempts || f->bq.kicks ||
        !io || !io->now_ns || !io->physical_counter || !io->read || !io->write_watchdog ||
        !freq || freq>UINT64_MAX/1000000000ULL || !deadline ||
        !timer || !exclusive || !receiver || !continuity) {
        f->bq.stopped=1; f->active=0; f->last_result=A04_BQ_STOPPED; return -1;
    }
    f->io=*io; f->frequency=freq; f->first_deadline=deadline;
    f->active=1; f->last_result=A04_BQ_STOPPED; return 0;
}
enum a04_bq_result a04_fw_tick(struct a04_fw_owner *f)
{
    struct a04_bq_bus bus;
    uint64_t deadline;
    if (!f || !f->active || f->exited || f->bq.stopped) return A04_BQ_STOPPED;
    if (f->bq.kicks) {
        if (f->bq.last_kick>UINT64_MAX-MAX_GAP) {
            f->bq.stopped=1; f->active=0; f->last_result=A04_BQ_DEADLINE; return f->last_result;
        }
        deadline=f->bq.last_kick+MAX_GAP;
    } else deadline=f->first_deadline;
    bus.context=f; bus.now_ns=fw_now; bus.read=fw_read; bus.write_watchdog=fw_kick;
    f->last_result=a04_bq_step(&f->bq,&bus,deadline);
    if (f->last_result!=A04_BQ_OK) f->active=0;
    return f->last_result;
}
static void put64(uint8_t *p,uint64_t v)
{ unsigned i; for(i=0;i<8;++i) p[i]=(uint8_t)(v>>(8u*i)); }
static uint64_t raw(const struct a04_bq_snapshot *s)
{ uint64_t v=0; unsigned i; for(i=0;i<8;++i) v|=(uint64_t)s->raw[i]<<(8u*i); return v; }
static uint64_t seal(const uint8_t *p)
{ uint64_t h=14695981039346656037ULL; unsigned i; for(i=0;i<120;++i){h^=p[i];h*=1099511628211ULL;} return h; }
static unsigned xnu_reason(enum a04_bq_result r)
{
    switch(r) {
    case A04_BQ_OK:return 0; case A04_BQ_DEADLINE:return 1;
    case A04_BQ_READ_ERROR:return 2; case A04_BQ_POLICY:return 3;
    case A04_BQ_FAULT:return 4; case A04_BQ_WRITE_ERROR:return 5;
    default:return 6;
    }
}
int a04_fw_exit(struct a04_fw_owner *f,uint64_t exit_tick,uint8_t *p,unsigned size)
{
    unsigned i, active;
    if (!f || f->exited) return -1;
    active=f->active; f->exited=1; f->active=0;
    if (!p || size!=128) return -1;
    if (active && f->last_result==A04_BQ_OK &&
        !a04_bq_publish_handoff(p,size,&f->bq,f->last_tick,exit_tick,f->frequency)) return 0;
    /* Preserve failure evidence in a sealed, expressly non-adoptable state. */
    for(i=0;i<128;++i) p[i]=0;
    put64(p,0x314f524147484334ULL); put64(p+8,128); put64(p+16,2);
    put64(p+24,f->last_tick); put64(p+32,f->frequency); put64(p+40,exit_tick);
    put64(p+48,f->bq.kicks); put64(p+56,f->bq.fault_history);
    put64(p+64,f->bq.first.valid); put64(p+72,raw(&f->bq.first));
    put64(p+80,f->bq.latest.valid); put64(p+88,raw(&f->bq.latest));
    put64(p+96,xnu_reason(f->last_result)); put64(p+120,seal(p));
    return -1;
}
