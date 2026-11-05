/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ24160_CHARGER_H
#define A04_BQ24160_CHARGER_H
#include <stdint.h>

/* Called only under the single I2C2 owner's lock, with an already armed
 * maintenance worker. Return zero only for a completely successful transfer. */
struct a04_bq_bus {
    void *context;
    uint64_t (*now_ns)(void *);
    int (*read)(void *, uint8_t reg, uint8_t *value);
    int (*write_watchdog)(void *, uint8_t value);
};
struct a04_bq_snapshot { uint8_t raw[8]; unsigned valid; uint64_t at; };
struct a04_bq_owner {
    struct a04_bq_snapshot first, latest;
    uint64_t last_kick;
    uint64_t handoff_deadline;
    uint64_t firmware_kicks;
    struct a04_bq_snapshot firmware_first, firmware_latest;
    int handoff_pending;
    unsigned attempts, kicks, fault_history;
    int stopped;
};
enum a04_bq_result {
    A04_BQ_OK, A04_BQ_INVALID, A04_BQ_STOPPED, A04_BQ_DEADLINE,
    A04_BQ_READ_ERROR, A04_BQ_POLICY, A04_BQ_FAULT, A04_BQ_WRITE_ERROR
};
/* Serialize a boot-local UniXNU v1 handoff after firmware ownership stops.
 * Caller serializes publisher against firmware I2C and exit; last_tick must
 * be captured for the same owner's most recent successful watchdog write.
 * A zeroed output is never a valid record. This routine does not create or
 * authenticate a Windows-visible transport or an out-of-D0 maintainer. */
int a04_bq_publish_handoff(uint8_t *out, unsigned size,
                           const struct a04_bq_owner *owner,
                           uint64_t last_tick, uint64_t exit_tick,
                           uint64_t frequency);
/* Decode the UniXNU v1 record as an offline contract, not proof of trust.
 * Caller must authenticate boot-local origin and ownership transfer separately.
 * now_ns is sampled BEFORE physical_counter, on a monotonic clock; this
 * conservative ordering cannot add time to the firmware's 20-second budget.
 * Failure latches stopped. The caller must be serialized and use a fresh owner. */
int a04_bq_adopt_handoff(struct a04_bq_owner *owner, const uint8_t *record,
                         unsigned size, uint64_t now_ns,
                         uint64_t physical_counter, uint64_t frequency);
/* Caller holds the same lock used by LC709203F for the entire cycle. The
 * worker must already be armed and a valid firmware handoff established.
 * Failure can leave an active watchdog and eventual hardware defaults. */
enum a04_bq_result a04_bq_step(struct a04_bq_owner *, const struct a04_bq_bus *, uint64_t deadline_ns);
#endif
