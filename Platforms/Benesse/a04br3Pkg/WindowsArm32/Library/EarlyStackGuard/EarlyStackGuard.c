/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Early SEC has no entropy provider; this detects corruption but is not secret. */
#include <Base.h>
#include <Library/BaseLib.h>
__UINTPTR_TYPE__ __stack_chk_guard = 0xa04b3816U;
VOID EFIAPI __stack_chk_fail (VOID) { CpuDeadLoop (); }
