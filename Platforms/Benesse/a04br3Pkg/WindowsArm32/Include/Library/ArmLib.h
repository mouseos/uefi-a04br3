/* ARM32 board compatibility with Mu's AArch64-only ArmLib public header.
 * Preserve upstream definitions, but use the ARM exception number for IRQ.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#pragma once
#include_next <Library/ArmLib.h>
#if defined (MDE_CPU_ARM)
#undef ARM_ARCH_EXCEPTION_IRQ
#define ARM_ARCH_EXCEPTION_IRQ EXCEPT_ARM_IRQ
#endif
