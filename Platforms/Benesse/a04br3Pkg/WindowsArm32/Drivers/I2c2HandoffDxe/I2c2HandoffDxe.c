/* SPDX-License-Identifier: MIT
 * Board-specific I2C2 setup derived from mouseos/edk2-mtk A04BR3Pkg
 * I2c2HandoffDxe. No charger or gauge transaction is issued.
 */
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>
#define TOP 0x10000000u
#define INFRA 0x10001000u
#define GPIO 0x10005000u
#define APMIXED 0x10018000u
#define I2C 0x1100b000u

STATIC EFI_STATUS ConfigureI2c2(VOID)
{
  UINT32 Ahb, Factor, Con0, Power, Pcw, Sample, Step, Best, A, B, Timing, Mux;
  UINT64 Parent, Vco;
  Ahb=(MmioRead32(TOP)>>4)&15u;
  if (Ahb==1) Parent=13000000u;
  else {
    Factor=Ahb==2 ? 11 : Ahb==4 ? 12 : Ahb==12 ? 10 : 0;
    if (!Factor) return EFI_UNSUPPORTED;
    Con0=MmioRead32(APMIXED+0x120); Power=MmioRead32(APMIXED+0x130);
    if (!(Con0&1u) || (Power&3u)!=1u) return EFI_NOT_READY;
    Pcw=MmioRead32(APMIXED+0x124);
    Vco=RShiftU64(MultU64x32(26000000u,Pcw&0x1fffffu)+16383u,14);
    Parent=DivU64x32(Vco+(1u<<((Pcw>>24)&7u))-1u,1u<<((Pcw>>24)&7u));
    Parent=DivU64x32(DivU64x32(Parent,Factor),2);
    if (!Parent || Pcw!=MmioRead32(APMIXED+0x124)) return EFI_NOT_READY;
  }
  Sample=0; Step=0; Best=513;
  for (A=1; A<=8; ++A) for (B=1; B<=64; ++B) {
    if (Parent<=MultU64x32(400000u,A*B) && A*B<Best) { Sample=A; Step=B; Best=A*B; }
  }
  if (!Sample) return EFI_UNSUPPORTED;
  MmioWrite32(TOP+0x84,BIT16);
  if (MmioRead32(TOP+0x24)&BIT16) return EFI_DEVICE_ERROR;
  if (MmioRead16(I2C+0x24)&1u) return EFI_NOT_READY;
  Mux=MmioRead32(INFRA+0x80);
  MmioWrite32(INFRA+0x80,Mux&~BIT3);
  if (MmioRead32(INFRA+0x80)&BIT3) return EFI_DEVICE_ERROR;
  MmioWrite32(GPIO+0x3c8,0x3f); MmioWrite32(GPIO+0x3c4,9);
  MmioWrite32(GPIO+0x914,2); MmioWrite32(GPIO+0x538,0x3000);
  MmioWrite16(I2C+0x08,0); MmioWrite16(I2C+0x0c,7); MmioWrite16(I2C+0x38,1);
  MmioWrite16(I2C+0x40,3); MmioWrite16(I2C+0x54,0); MmioWrite16(I2C+0x70,1);
  Timing=((Sample-1)<<8)|(Step-1);
  MmioWrite16(I2C+0x20,(UINT16)Timing); MmioWrite16(I2C+0x48,0);
  MmioWrite16(I2C+0x28,0x8001); MmioWrite16(I2C+0x60,0);
  MmioWrite16(I2C+0x10,0x28); MmioWrite16(I2C+0x1c,2);
  if ((MmioRead32(INFRA+0x80)&BIT3) || (MmioRead32(GPIO+0x3c0)&0x3f)!=9 ||
      (MmioRead32(GPIO+0x530)&0x3000) || MmioRead16(I2C+0x70)!=1 ||
      MmioRead16(I2C+0x40)!=3 || MmioRead16(I2C+0x28)!=0x8001 ||
      MmioRead16(I2C+0x20)!=Timing) return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}

STATIC VOID EFIAPI BeforeExit(EFI_EVENT Event, VOID *Context)
{
  EFI_STATUS Status;
  (VOID)Event; (VOID)Context;
  Status = ConfigureI2c2();
  DEBUG ((DEBUG_ERROR, "A04BAT UEFI I2C2 setup=%r\n", Status));
}
EFI_STATUS EFIAPI I2c2HandoffEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;
  (VOID)ImageHandle; (VOID)SystemTable;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, BeforeExit, NULL,
                           &gEfiEventBeforeExitBootServicesGuid, &Event);
}
