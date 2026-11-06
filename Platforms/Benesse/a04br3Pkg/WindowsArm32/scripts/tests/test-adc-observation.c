/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "../../Drivers/ThermalHandoffDxe/AdcObservation.h"
int main(void) {
 A04_ADC_OBSERVATION v,old;memset(&v,0xa5,sizeof(v));old=v;
 assert(A04AdcObservationBuild(0,1,&v) && !memcmp(&v,&old,sizeof(v)));
 assert(A04AdcObservationBuild(0xffffffffu,0xffffffffu,&v) && !memcmp(&v,&old,sizeof(v)));
 assert(!A04AdcObservationBuild(0x40000002u,0x40000002u,&v));
 assert(v.version==1 && v.bytes==24 && v.phase==1 && v.clock_first==v.clock_second && v.secure_release==0);
 return 0;
}
