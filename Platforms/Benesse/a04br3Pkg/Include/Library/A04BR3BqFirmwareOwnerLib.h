/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_FIRMWARE_OWNER_H
#define A04_BQ_FIRMWARE_OWNER_H
#include <Library/A04BR3BqChargerCore.h>

/* Portable board-only owner. All calls including EBS must be serialized by the
 * platform, and all I2C2 clients must share the same exclusion. Admission is
 * not established by these booleans: their caller must establish the actual
 * firmware/OS, D0-exit and fault ownership contracts before passing true. */
struct a04_fw_io {
    void *context;
    uint64_t (*now_ns)(void *);
    uint64_t (*physical_counter)(void *);
    int (*read)(void *, uint8_t, uint8_t *);
    int (*write_watchdog)(void *, uint8_t);
};
struct a04_fw_owner {
    struct a04_bq_owner bq;
    struct a04_fw_io io;
    uint64_t frequency, last_tick, first_deadline;
    enum a04_bq_result last_result;
    unsigned active, exited;
};
/* Caller zero-initializes state; first_deadline is an absolute monotonic
 * deadline established from the prior owner, not a new timeout. */
int a04_fw_start(struct a04_fw_owner *, const struct a04_fw_io *, uint64_t frequency,
                 uint64_t first_deadline, int timer_ready, int exclusive_bus,
                 int receiver_ready, int power_and_fault_continuity);
enum a04_bq_result a04_fw_tick(struct a04_fw_owner *);
/* EBS: caller supplies the physical exit counter under the same exclusion.
 * No I2C, allocation, timer, or firmware callbacks are invoked. Writes one
 * boot-local 128-byte record; stopped records are sealed but not adoptable. */
int a04_fw_exit(struct a04_fw_owner *, uint64_t exit_tick, uint8_t *out, unsigned size);
#endif
