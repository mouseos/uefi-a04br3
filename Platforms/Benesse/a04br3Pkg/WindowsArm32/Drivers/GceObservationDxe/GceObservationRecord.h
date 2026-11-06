// SPDX-License-Identifier: MIT
#ifndef A04_GCE_OBSERVATION_RECORD_H
#define A04_GCE_OBSERVATION_RECORD_H
#include <stdint.h>
/* Observations only. UINT32_MAX marks a register not read; ownership is
 * always zero. Clock state is not proof of power, security, or exclusivity. */
typedef struct {
  uint32_t version, bytes, valid, top2_status;
  uint32_t thread0_enable, thread0_status, thread0_irq, ownership;
} A04_GCE_OBSERVATION_RECORD;
#define A04_GCE_OBSERVATION_TOP2 1u
#endif
