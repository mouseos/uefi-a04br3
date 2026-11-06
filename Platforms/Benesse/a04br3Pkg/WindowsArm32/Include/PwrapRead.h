/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Shared bounded WACS2 read transport, extracted from TouchPowerDxe.
 * Command/valid-clear writes below do not modify PMIC registers. */
#ifndef A04BR3_PWRAP_READ_H
#define A04BR3_PWRAP_READ_H
#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#define WACS_CMD 0x1000F0A0U
#define WACS_DATA 0x1000F0A4U
#define WACS_CLEAR 0x1000F0A8U
STATIC EFI_STATUS WaitFsm(UINT32 Target) {
  UINTN n;
  for (n=0; n<10000; ++n) {
    if (((MmioRead32(WACS_DATA)>>16)&7U)==Target) return EFI_SUCCESS;
    MicroSecondDelay(1);
  }
  return EFI_TIMEOUT;
}
STATIC EFI_STATUS ReadPmic(UINT16 Address, UINT16 *Value) {
  EFI_STATUS Status=WaitFsm(0);
  if (EFI_ERROR(Status)) return Status;
  MmioWrite32(WACS_CMD, ((UINT32)Address>>1)<<16);
  Status=WaitFsm(6);
  if (EFI_ERROR(Status)) return Status;
  *Value=(UINT16)MmioRead32(WACS_DATA);
  MmioWrite32(WACS_CLEAR,1);
  return WaitFsm(0);
}
#endif
