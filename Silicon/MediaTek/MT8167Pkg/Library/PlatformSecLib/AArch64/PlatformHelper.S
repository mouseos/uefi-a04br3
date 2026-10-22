//
// Copyright (c) 2012-2013, ARM Limited. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// On A04BR3 the MT8167 LK enters the kernel slot through ATF at EL2 (AArch64,
// SPSR 0x3C9; see the A04BR3 edk2 handoff investigation). DarwinBoot refuses
// to load XNU unless the firmware runs at EL1 (CurrentEL == 4), so leave EL2
// here, as MT6768Pkg PlatformSecLib does. The A04BR3 XNU boot shim makes the
// same EL2 -> EL1h drop on this board.
//
// Before dropping to EL1:
//  - HCR_EL2 = RW (bit 31): EL1 is AArch64, nothing else trapped or routed.
//  - CNTHCTL_EL2 = EL1PCTEN | EL1PCEN: EL1 may use the physical counter and
//    timer (USE_PHYSICAL_TIMER). edk2-mtk MediaTekPkg/PrePi/Pi.c sets the same
//    bits on this board.
//  - CNTVOFF_EL2 = 0: virtual and physical counters agree.
//  - CPTR_EL2 = RES1 bits only (0x33FF, non-VHE layout): no FP/SIMD trap to
//    EL2, which would have no handler. HSTR_EL2 = 0: no CP15 traps.
//  - SCTLR_EL1 = RES1 bits only (MMU, caches and alignment checks off,
//    little endian), the value Linux uses as INIT_SCTLR_EL1_MMU_OFF.
//  - VPIDR_EL2 = MIDR_EL1, VMPIDR_EL2 = MPIDR_EL1: with EL2 implemented,
//    Non-secure EL1 reads of MIDR_EL1 and MPIDR_EL1 return VPIDR_EL2 and
//    VMPIDR_EL2, whose reset values are UNKNOWN. Copy the real values, as
//    Linux does before it leaves EL2 (arch/arm64/include/asm/el2_setup.h,
//    __init_el2_nvhe_idregs). XNU reads MPIDR_EL1 at EL1 to identify the boot
//    CPU; MIDR_EL1 selects CPU errata handling.
//

#include <AsmMacroLib.h>

ASM_FUNC (PlatformAssemblyInitialize)
  EL1_OR_EL2 (x0)
1:ret
2:mrs   x0, midr_el1
  msr   vpidr_el2, x0
  mrs   x0, mpidr_el1
  msr   vmpidr_el2, x0

  mov   x0, #(1 << 31)
  msr   hcr_el2, x0

  mov   x0, #0x3
  msr   cnthctl_el2, x0
  msr   cntvoff_el2, xzr

  mov   x0, #0x33FF
  msr   cptr_el2, x0
  msr   hstr_el2, xzr

  movz  x0, #0x0800
  movk  x0, #0x30D0, lsl #16
  msr   sctlr_el1, x0

  msr   elr_el2, x30

  mov   x0, #0x3C5 // EL1h, DAIF masked
  msr   spsr_el2, x0
  isb
  eret
