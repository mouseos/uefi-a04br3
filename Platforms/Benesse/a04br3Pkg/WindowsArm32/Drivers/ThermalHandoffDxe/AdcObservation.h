// SPDX-License-Identifier: MIT
#ifndef A04_ADC_OBSERVATION_H
#define A04_ADC_OBSERVATION_H
#include <stdint.h>
/* Early DXE clock snapshot. It is neither an EBS handoff nor an EL3 release. */
typedef struct { uint32_t version,bytes,phase,clock_first,clock_second,secure_release; } A04_ADC_OBSERVATION;
static inline int A04AdcObservationBuild(uint32_t first,uint32_t second,A04_ADC_OBSERVATION *out)
{
 A04_ADC_OBSERVATION v;
 if(!out || first==0xffffffffu || first!=second)return -1;
 v.version=1;v.bytes=sizeof(v);v.phase=1;v.clock_first=first;v.clock_second=second;v.secure_release=0;
 *out=v;return 0;
}
#endif
