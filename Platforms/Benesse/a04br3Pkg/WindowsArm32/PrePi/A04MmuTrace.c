// A04BR3 test-only MMU transition tracing; upstream EDK II remains unchanged.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Base.h>
#include <Library/DebugLib.h>
#include <A04ArmLib.h>
#include <Library/A04BR3TestCycleLib.h>

VOID EFIAPI __real_ArmEnableMmu (VOID);

static UINT32 CheckTranslation (UINT32 Address)
{
  UINT32 Par;
  __asm__ volatile ("mcr p15, 0, %1, c7, c8, 0; isb; mrc p15, 0, %0, c7, c4, 0" : "=r" (Par) : "r" (Address) : "memory");
  return Par;
}

static VOID DumpMapping (UINT32 *Table, UINT32 Address, CONST CHAR8 *Name)
{
  UINT32 Entry = Table[Address >> 20];
  DEBUG ((DEBUG_ERROR, "A04MAP %a %08x l1=%08x", Name, Address, Entry));
  if ((Entry & 3) == 1) {
    UINT32 *L2 = (UINT32 *)(UINTN)(Entry & 0xfffffc00);
    DEBUG ((DEBUG_ERROR, " l2=%08x", L2[(Address >> 12) & 255]));
  }
  DEBUG ((DEBUG_ERROR, " par=%08x\n", CheckTranslation (Address)));
}

VOID EFIAPI __wrap_ArmEnableMmu (VOID)
{
  UINT32 Ttbr, Sp, Ttbcr, Dacr, Sctlr;
  __asm__ volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (Ttbr));
  __asm__ volatile ("mov %0, sp" : "=r" (Sp));
  __asm__ volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r" (Ttbcr));
  __asm__ volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r" (Dacr));
  __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (Sctlr));
  DEBUG ((DEBUG_ERROR, "A04MAP ttbcr=%08x dacr=%08x sctlr=%08x\n", Ttbcr,Dacr,Sctlr));
  UINT32 *Table = (UINT32 *)(UINTN)(Ttbr & 0xffffc000);
  DEBUG ((DEBUG_ERROR, "A04MAP table=%08x\n", Ttbr));
  DumpMapping (Table, (UINT32)(UINTN)__wrap_ArmEnableMmu, "pc");
  DumpMapping (Table, Sp, "sp");
  DumpMapping (Table, 0x11005000, "uart");
  DEBUG ((DEBUG_ERROR, "A04MAP disable caches before MMU transition\n"));
  ArmDisableDataCache ();
  ArmDisableInstructionCache ();
  __real_ArmEnableMmu ();
  DEBUG ((DEBUG_ERROR, "A04MAP returned from MMU enable\n"));
}
