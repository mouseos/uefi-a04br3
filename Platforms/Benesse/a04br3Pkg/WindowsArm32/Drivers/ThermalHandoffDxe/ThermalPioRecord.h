// SPDX-License-Identifier: MIT
#ifndef A04_THERMAL_PIO_RECORD_H
#define A04_THERMAL_PIO_RECORD_H
#include <stdint.h>
#define A04_THERMAL_PIO_NAME L"A04BR3ThermalPioHandoff"
#define A04_THERMAL_PIO_GUID {0x79c36824,0x7e48,0x4ec8,{0xb5,0xe7,0x57,0xf8,0xae,0x04,0x10,0x16}}
/* Boot-local normal-world producer takeover; not a claim about secure firmware.
 * Ready records leave the thermal bus master held in reset and both clocks on.
 * No IRQ is used by the Windows PIO consumer. All fields UINT32, attrs exactly6. */
typedef struct {
 uint32_t version,bytes,ready,stage,clock_before,clock_after,reset_before,reset_after;
 uint32_t irq_before,irq_after,ahb_before,ahb_after,mon_after,pause_after;
 uint32_t adc_mode,adc_trigger,adc_busy,irq_pending;
} A04_THERMAL_PIO_RECORD;
#endif
