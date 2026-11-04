// Copyright (c) 2026. SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Library/PcdLib.h>
#include <AsmMacroIoLib.h>
#ifndef ASM_PFX
#define ASM_PFX(name) name
#endif
.text
.arm
.align 2
ASM_FUNC(_ModuleEntryPoint)
  cpsid aif
  mov r4, r0  // Preserve LK boot argument across early AAPCS calls.
  ldr r0, =FixedPcdGet32(PcdPrePiStackBase)
  ldr r1, =FixedPcdGet32(PcdPrePiStackSize)
  add sp, r0, r1
  // Separate stack banks let early aborts report through the serial port.
  cps #0x17
  add sp, r0, #0x1000
  cps #0x1b
  add sp, r0, #0x2000
  cps #0x13
  adr r2, A04Vectors
  mcr p15, 0, r2, c12, c0, 0
  isb
  bl ASM_PFX(ArmDisableCachesAndMmu)
  bl ASM_PFX(ArmInvalidateInstructionCache)
  bl ASM_PFX(ArmInvalidateTlb)
  ldr r0, =FixedPcdGet32(PcdPrePiStackBase)
  ldr r1, =FixedPcdGet32(PcdPrePiStackSize)
  mov r2, r4
  bl ASM_PFX(CEntryPoint)
1: b 1b

.align 5
A04Vectors:
  b A04Unknown
  b A04Undef
  b A04Unknown
  b A04Prefetch
  b A04Data
  b A04Unknown
  b A04Unknown
  b A04Unknown
A04Undef:
  mov r0, #1
  b A04Fault
A04Prefetch:
  mov r0, #3
  b A04Fault
A04Data:
  mov r0, #4
  b A04Fault
A04Unknown:
  mov r0, #0
A04Fault:
  mov r1, lr
  mrs r2, spsr
  bl ASM_PFX(A04EarlyFault)
2: b 2b
