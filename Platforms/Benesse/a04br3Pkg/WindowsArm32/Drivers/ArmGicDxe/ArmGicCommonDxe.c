/* Board-only ARM32 adaptation of the upstream GIC driver.
 * Mu's ArmLib defines the AArch64 IRQ vector; this module runs AArch32.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <Library/ArmLib.h>
#include <Protocol/DebugSupport.h>
#undef ARM_ARCH_EXCEPTION_IRQ
#define ARM_ARCH_EXCEPTION_IRQ EXCEPT_ARM_IRQ
STATIC_ASSERT (ARM_ARCH_EXCEPTION_IRQ == 6, "ARM32 IRQ vector mismatch");
#include <Drivers/ArmGicDxe/ArmGicCommonDxe.c>
