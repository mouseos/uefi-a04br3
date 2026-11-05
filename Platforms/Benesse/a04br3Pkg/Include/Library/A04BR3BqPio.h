/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_PIO_H
#define A04_BQ_PIO_H
#include <stdint.h>
struct a04_bq_pio {
    void *context;
    uint16_t (*read16)(void *, unsigned);
    void (*write16)(void *, unsigned, uint16_t);
    uint64_t (*now_ns)(void *);
    void (*delay_us)(void *, unsigned);
    int faulted;
};
int a04_bq_pio_read(void *, uint8_t reg, uint8_t *value);
int a04_bq_pio_kick(void *, uint8_t value);
#endif
