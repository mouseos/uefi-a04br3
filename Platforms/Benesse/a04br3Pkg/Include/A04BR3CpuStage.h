/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_CPU_STAGE_H
#define A04BR3_CPU_STAGE_H

/* Read-only snapshots: not an atomic state or a measured CPU frequency. */
typedef UINT32 (EFIAPI *A04BR3_CPU_STAGE_READ32)(UINTN Address);
typedef struct {
  UINT32 InfraMux;
  UINT32 ArmDcm;
  UINT32 ArmPllCon0;
  UINT32 ArmPllCon1;
  UINT32 ArmPllPower;
  UINT32 BusDcmControl;
  UINT32 BusDcm0;
  UINT32 BusDcm1;
} A04BR3_CPU_STAGE;

STATIC inline A04BR3_CPU_STAGE
A04BR3CpuStageRead (A04BR3_CPU_STAGE_READ32 Read32)
{
  A04BR3_CPU_STAGE State;
  State.InfraMux      = Read32 (0x10001000U);
  State.ArmDcm        = Read32 (0x10001014U);
  State.ArmPllCon0    = Read32 (0x10018100U);
  State.ArmPllCon1    = Read32 (0x10018104U);
  State.ArmPllPower   = Read32 (0x10018110U);
  State.BusDcmControl = Read32 (0x10000010U);
  State.BusDcm0       = Read32 (0x10000028U);
  State.BusDcm1       = Read32 (0x1000002cU);
  return State;
}
#endif
