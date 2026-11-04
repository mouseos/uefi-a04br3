/* SPDX-License-Identifier: MIT
 * Board-only experimental I2C1 setup. Before ExitBootServices is the last
 * firmware access. No sensor or camera transfer is performed here.
 * Adapted from A04BR3 I2c2HandoffDxe; see repository license.
 */
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>
#ifndef A04_I2C1_HOST_TEST
#include <Protocol/AcpiTable.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include "Mma8652Aml.h"
STATIC volatile UINT32 *mStatus;
STATIC UINT32 mTiming;
#endif
#define TOP 0x10000000u
#define INFRA 0x10001000u
#define GPIO 0x10005000u
#define APMIXED 0x10018000u
#define I2C 0x1100a000u

STATIC EFI_STATUS ConfigureI2c1(VOID)
{
  UINT32 Ahb, Factor, Con0, Power, Pcw, Sample, Step, Best, A, B, Timing, Mux, Pins;
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
  /* Keep bus at or below 100 kHz, as in the board camera bring-up. */
  for (A=1; A<=8; ++A) for (B=1; B<=64; ++B) {
    if (Parent<=MultU64x32(200000u,A*B) && A*B<Best) { Sample=A; Step=B; Best=A*B; }
  }
  if (!Sample) return EFI_UNSUPPORTED;
  MmioWrite32(TOP+0x84,BIT4);
  if (MmioRead32(TOP+0x24)&BIT4) return EFI_DEVICE_ERROR;
  /* Do not touch controller registers before its gate has been verified. */
  if (MmioRead16(I2C+0x24)&1u) return EFI_NOT_READY;
  Mux=MmioRead32(INFRA+0x80);
  MmioWrite32(INFRA+0x80,Mux&~BIT2);
  if (MmioRead32(INFRA+0x80)&BIT2) return EFI_DEVICE_ERROR;
  /* GPIO52/53 mode 1 and bias disabled; SET/CLR avoid shared-word RMW. */
  MmioWrite32(GPIO+0x3a8,(7u<<6)|(7u<<9));
  MmioWrite32(GPIO+0x3a4,(1u<<6)|(1u<<9));
  MmioWrite32(GPIO+0x538,(1u<<4)|(1u<<5));
  MmioWrite32(GPIO+0x914,BIT0);
  Pins=MmioRead32(GPIO+0x3a0);
  if ((Pins&((7u<<6)|(7u<<9)))!=((1u<<6)|(1u<<9)) ||
      (MmioRead32(GPIO+0x530)&((1u<<4)|(1u<<5))) ||
      !(MmioRead32(GPIO+0x910)&BIT0)) return EFI_DEVICE_ERROR;
  MmioWrite16(I2C+0x08,0); MmioWrite16(I2C+0x0c,7); MmioWrite16(I2C+0x38,1);
  MmioWrite16(I2C+0x40,3); MmioWrite16(I2C+0x54,0); MmioWrite16(I2C+0x70,1);
  Timing=((Sample-1)<<8)|(Step-1);
  MmioWrite16(I2C+0x20,(UINT16)Timing); MmioWrite16(I2C+0x48,0);
  MmioWrite16(I2C+0x28,0x8001); MmioWrite16(I2C+0x60,0);
  MmioWrite16(I2C+0x10,0x28); MmioWrite16(I2C+0x1c,2);
  if (MmioRead16(I2C+0x70)!=1 || MmioRead16(I2C+0x40)!=3 ||
      MmioRead16(I2C+0x28)!=0x8001 || MmioRead16(I2C+0x20)!=Timing ||
      (MmioRead16(I2C+0x24)&1u)) return EFI_DEVICE_ERROR;
#ifndef A04_I2C1_HOST_TEST
  mTiming=Timing;
#endif
  return EFI_SUCCESS;
}
STATIC VOID EFIAPI BeforeExit(EFI_EVENT Event, VOID *Context)
{
  EFI_STATUS Status;
  (VOID)Event; (VOID)Context;
#ifndef A04_I2C1_HOST_TEST
  mStatus[0]=0; mStatus[1]=0; mStatus[2]=0;
  WriteBackDataCacheRange((VOID *)mStatus, 3*sizeof(*mStatus));
#endif
  Status=ConfigureI2c1();
#ifndef A04_I2C1_HOST_TEST
  if (!EFI_ERROR(Status)) {
    mStatus[1]=mTiming; mStatus[2]=~mTiming;
    WriteBackDataCacheRange((VOID *)mStatus, 3*sizeof(*mStatus));
    mStatus[0]=0x41303431u;
  }
  WriteBackDataCacheRange((VOID *)mStatus, 3*sizeof(*mStatus));
#endif
  DEBUG((DEBUG_ERROR,"A04ACC UEFI I2C1 setup=%r\n",Status));
}
#ifndef A04_I2C1_HOST_TEST
STATIC BOOLEAN PatchAddress(UINT8 *Aml, UINTN Size, UINT32 Pattern, UINT32 Address)
{
  UINTN I, Count=0;
  for (I=36; I+4<=Size; ++I) {
    if (Aml[I]==(UINT8)Pattern && Aml[I+1]==(UINT8)(Pattern>>8) &&
        Aml[I+2]==(UINT8)(Pattern>>16) && Aml[I+3]==(UINT8)(Pattern>>24)) {
      Aml[I]=(UINT8)Address; Aml[I+1]=(UINT8)(Address>>8);
      Aml[I+2]=(UINT8)(Address>>16); Aml[I+3]=(UINT8)(Address>>24); ++Count;
    }
  }
  return Count==1;
}
EFI_STATUS EFIAPI I2c1HandoffEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event; EFI_STATUS Status; EFI_PHYSICAL_ADDRESS Page=MAX_UINT32;
  EFI_ACPI_TABLE_PROTOCOL *Tables; UINT8 Aml[sizeof(mMma8652Aml)], Sum=0;
  UINTN I, Key;
  (VOID)ImageHandle; (VOID)SystemTable;
  Status=gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid,NULL,(VOID **)&Tables);
  if (EFI_ERROR(Status)) return Status;
  Status=gBS->AllocatePages(AllocateMaxAddress,EfiACPIMemoryNVS,1,&Page);
  if (EFI_ERROR(Status)) return Status;
  mStatus=(volatile UINT32 *)(UINTN)Page; mStatus[0]=0; mStatus[1]=0; mStatus[2]=0;
  WriteBackDataCacheRange((VOID *)mStatus,3*sizeof(*mStatus));
  CopyMem(Aml,mMma8652Aml,sizeof(Aml));
  if (!PatchAddress(Aml,sizeof(Aml),0x12345678u,(UINT32)Page) ||
      !PatchAddress(Aml,sizeof(Aml),0x87654321u,(UINT32)Page)) {
    gBS->FreePages(Page,1); return EFI_COMPROMISED_DATA;
  }
  Aml[9]=0; for (I=0; I<sizeof(Aml); ++I) Sum=(UINT8)(Sum+Aml[I]); Aml[9]=(UINT8)(0-Sum);
  Status=gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,BeforeExit,NULL,
                            &gEfiEventBeforeExitBootServicesGuid,&Event);
  if (EFI_ERROR(Status)) { gBS->FreePages(Page,1); return Status; }
  Status=Tables->InstallAcpiTable(Tables,Aml,sizeof(Aml),&Key);
  if (EFI_ERROR(Status)) { gBS->CloseEvent(Event); gBS->FreePages(Page,1); }
  return Status;
}
#endif
