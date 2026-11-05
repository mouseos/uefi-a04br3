/* SPDX-License-Identifier: MIT
 * Policy derived from UniXNU-a04br3 A04BR3ChargerCore.h (MIT), commit
 * 0420ce37f0241178a7276784bbc013b0dbba4c95. No platform I/O here.
 */
#include <Library/A04BR3BqChargerCore.h>
#define SECOND_NS 1000000000ULL
#define MAX_GAP_NS (20ULL * SECOND_NS)

static enum a04_bq_result policy(const struct a04_bq_snapshot *s)
{
    unsigned status;
    if (s->valid != 0xffu || (s->raw[4] & 0xf8u) != 0x40u) return A04_BQ_POLICY;
    if ((s->raw[0] & 7u) || (s->raw[7] & 6u)) return A04_BQ_FAULT;
    if ((s->raw[0] & 8u) || (s->raw[2] & 0x7fu) != 0x0cu ||
        (s->raw[3] & 0xfeu) != 0xa4u || s->raw[5] != 0x51u ||
        (s->raw[7] & 0xe9u) != 0x28u || (s->raw[6] & 0x3fu) ||
        (s->raw[1] & 15u)) return A04_BQ_POLICY;
    status = (s->raw[0] >> 4) & 7u;
    if ((status == 3u && !(s->raw[1] & 0xc0u)) ||
        (status == 4u && !(s->raw[1] & 0x30u)) ||
        (status == 5u && (!(s->raw[1] & 0xc0u) || !(s->raw[1] & 0x30u))))
        return A04_BQ_OK;
    return A04_BQ_POLICY;
}
static int late(const struct a04_bq_owner *o, uint64_t now, uint64_t deadline)
{
    return !deadline || now >= deadline ||
        (o->handoff_pending && now >= o->handoff_deadline) ||
        (o->kicks && (now < o->last_kick || now - o->last_kick >= MAX_GAP_NS));
}
enum a04_bq_result a04_bq_step(struct a04_bq_owner *o, const struct a04_bq_bus *b, uint64_t deadline)
{
    struct a04_bq_snapshot s = {{0}, 0, 0};
    enum a04_bq_result result;
    uint64_t end, previous, kick_started;
    unsigned reg, first;
    if (!o || !b || !b->now_ns || !b->read || !b->write_watchdog) return A04_BQ_INVALID;
    if (o->stopped) return A04_BQ_STOPPED;
    s.at = b->now_ns(b->context);
    if (late(o, s.at, deadline)) { o->stopped = 1; return A04_BQ_DEADLINE; }
    previous = s.at;
    first = o->attempts == 0;
    for (reg = 0; reg < 8; ++reg) {
        end = b->now_ns(b->context);
        if (end < previous || end - s.at >= SECOND_NS || late(o, end, deadline)) {
            o->latest = s; if (first && o->attempts) o->first = s;
            o->stopped = 1; return A04_BQ_DEADLINE;
        }
        previous = end;
        if (!reg) ++o->attempts;
        if (b->read(b->context, (uint8_t)reg, &s.raw[reg])) {
            o->latest = s; if (first) o->first = s;
            o->stopped = 1; return A04_BQ_READ_ERROR;
        }
        s.valid |= 1u << reg;
        if (!reg) o->fault_history |= 1u << (s.raw[0] & 7u);
    }
    o->latest = s; if (first) o->first = s;
    result = policy(&s);
    end = b->now_ns(b->context);
    if (end < previous || end - s.at >= SECOND_NS || late(o, end, deadline)) result = A04_BQ_DEADLINE;
    if (result != A04_BQ_OK) { o->stopped = 1; return result; }
    /* Never echo status/fault bits, or write any charge configuration. */
    /* Conservatively start the next budget before the write transaction. */
    kick_started = end;
    if (b->write_watchdog(b->context, (uint8_t)(0x80u | (s.raw[0] & 8u)))) {
        o->stopped = 1; return A04_BQ_WRITE_ERROR;
    }
    end = b->now_ns(b->context);
    if (end < kick_started || end - s.at >= SECOND_NS || late(o, end, deadline)) {
        o->stopped = 1; return A04_BQ_DEADLINE;
    }
    o->last_kick = kick_started; ++o->kicks; o->handoff_pending = 0;
    return A04_BQ_OK;
}

/* Format and validation derived from A04BR3ChargerHandoff.h, MIT.  This
 * parser does not establish that Windows received the record from firmware. */
static uint64_t handoff_u64(const uint8_t *p)
{
    uint64_t v=0; unsigned i;
    for (i=0;i<8;++i) v |= (uint64_t)p[i] << (8u*i);
    return v;
}
static uint64_t handoff_seal(const uint8_t *p)
{
    uint64_t h=14695981039346656037ULL; unsigned i;
    for (i=0;i<120;++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}
static void handoff_snapshot(struct a04_bq_snapshot *s, uint64_t word, unsigned valid)
{
    unsigned i; s->valid=valid; s->at=0;
    for (i=0;i<8;++i) s->raw[i]=(uint8_t)(word >> (8u*i));
}
static void handoff_put64(uint8_t *p, uint64_t value)
{
    unsigned i; for (i=0;i<8;++i) p[i]=(uint8_t)(value >> (8u*i));
}
static uint64_t handoff_raw(const struct a04_bq_snapshot *s)
{
    uint64_t value=0; unsigned i;
    for (i=0;i<8;++i) value |= (uint64_t)s->raw[i] << (8u*i);
    return value;
}
int a04_bq_publish_handoff(uint8_t *p, unsigned size, const struct a04_bq_owner *o,
                           uint64_t last, uint64_t exited, uint64_t freq)
{
    unsigned i;
    if (!p || size != 128) return -1;
    for (i=0;i<128;++i) p[i]=0;
    if (!o || o->stopped || o->handoff_pending || !o->kicks ||
        !freq || freq > UINT64_MAX/1000000000ULL || last>exited ||
        exited-last>=20ULL*freq || (o->fault_history & ~1u) ||
        o->first.valid!=255 || (o->first.raw[0]&7u) || policy(&o->latest)!=A04_BQ_OK)
        return -1;
    handoff_put64(p,0x314f524147484334ULL); handoff_put64(p+8,128);
    handoff_put64(p+16,1); handoff_put64(p+24,last); handoff_put64(p+32,freq);
    handoff_put64(p+40,exited); handoff_put64(p+48,o->kicks);
    handoff_put64(p+56,o->fault_history); handoff_put64(p+64,o->first.valid);
    handoff_put64(p+72,handoff_raw(&o->first)); handoff_put64(p+80,o->latest.valid);
    handoff_put64(p+88,handoff_raw(&o->latest)); handoff_put64(p+120,handoff_seal(p));
    return 0;
}
int a04_bq_adopt_handoff(struct a04_bq_owner *o, const uint8_t *p,
                         unsigned size, uint64_t now_ns, uint64_t counter, uint64_t freq)
{
    uint64_t last, exited, delta, fractional, elapsed, remaining;
    struct a04_bq_snapshot first = {{0},0,0}, latest = {{0},0,0};
    if (!o) return -1;
    if (o->stopped || o->attempts || o->kicks || o->handoff_pending ||
        !p || size != 128 || handoff_u64(p) != 0x314f524147484334ULL ||
        handoff_u64(p+8) != 128 || handoff_u64(p+16) != 1 ||
        handoff_u64(p+120) != handoff_seal(p) || !freq ||
        freq > UINT64_MAX/1000000000ULL || handoff_u64(p+32) != freq ||
        !handoff_u64(p+48) || (handoff_u64(p+56) & ~1ULL) ||
        handoff_u64(p+64) != 255 || handoff_u64(p+80) != 255 ||
        handoff_u64(p+96) != 0 || handoff_u64(p+104) || handoff_u64(p+112)) goto fail;
    last=handoff_u64(p+24); exited=handoff_u64(p+40);
    if (last>exited || exited>counter || counter-last>=20ULL*freq) goto fail;
    handoff_snapshot(&first,handoff_u64(p+72),255);
    handoff_snapshot(&latest,handoff_u64(p+88),255);
    if ((first.raw[0]&7u) || policy(&latest)!=A04_BQ_OK) goto fail;
    delta=counter-last;
    fractional=(delta%freq)*1000000000ULL;
    elapsed=(delta/freq)*1000000000ULL+fractional/freq+(fractional%freq!=0);
    if (elapsed>=MAX_GAP_NS) goto fail;
    remaining=MAX_GAP_NS-elapsed;
    if (now_ns>UINT64_MAX-remaining) goto fail;
    o->firmware_first=first; o->firmware_latest=latest;
    o->firmware_kicks=handoff_u64(p+48);
    o->fault_history=(unsigned)handoff_u64(p+56);
    o->handoff_deadline=now_ns+remaining; o->handoff_pending=1;
    return 0;
fail:
    o->stopped=1;
    return -1;
}
