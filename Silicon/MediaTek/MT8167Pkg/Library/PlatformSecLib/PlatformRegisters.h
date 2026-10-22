#ifndef _PLATFORM_REGISTERS_H_
#define _PLATFORM_REGISTERS_H_

//
// TOPRGU watchdog mode register: writing the key with the enable bit clear
// stops the watchdog. Same write as edk2-mtk MediaTekPkg/PrePi/Pi.c
// (DisableWDT: 0x10007000 <- 0x22000000) used on A04BR3.
//
#define WDT_MODE_KEY                   0x22000000

#endif /* _PLATFORM_REGISTERS_H_ */
