// SPDX-License-Identifier: MIT
#ifndef A04_THERMAL_HANDOFF_RECORD_H
#define A04_THERMAL_HANDOFF_RECORD_H
#include <stdint.h>
/* All ownership flags remain zero until independently evidenced. */
typedef struct { uint32_t version,bytes,observation,ownership; } A04_THERMAL_HANDOFF_RECORD;
#define A04_THERMAL_RECORD_UNKNOWN {1u,16u,1u,0u}
#endif
