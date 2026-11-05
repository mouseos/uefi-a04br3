/* SPDX-License-Identifier: MIT
 * I2C2 clock, pin and timing setup from the MIT-licensed board-specific
 * UniXNU-a04br3 A04BR3ChargerBoardCore.h. No charger/gauge transaction.
 * BeforeExitBootServices runs while this UEFI remains sole I2C2 owner.
 */
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/A04BR3BqFirmwareOwnerLib.h>
#include <Library/A04BR3BqPio.h>
#include <Library/ArmLib.h>
#include <Library/TimerLib.h>
#ifndef A04BR3_BQ_HANDOFF
#define A04BR3_BQ_HANDOFF 0
#endif
#if A04BR3_BQ_HANDOFF
#include "BqHandoffAml.h"
STATIC struct a04_fw_owner mBqOwner;
STATIC struct a04_bq_pio mBqPio;
STATIC VOID BqTryStart(VOID);
STATIC BOOLEAN BqHardwareReady(VOID);
STATIC UINT8 *mBqRecord;
STATIC EFI_EVENT mBqTimer, mBqExit;
STATIC VOID EFIAPI BqTimer(EFI_EVENT Event, VOID *Context)
{
  (VOID)Event; (VOID)Context;
  if (mBqOwner.active && !BqHardwareReady()) {
    mBqOwner.bq.stopped = 1; mBqOwner.active = 0;
    mBqOwner.last_result = A04_BQ_STOPPED; return;
  }
  (VOID)a04_fw_tick(&mBqOwner);
}
STATIC VOID EFIAPI BqExit(EFI_EVENT Event, VOID *Context)
{
  (VOID)Event; (VOID)Context;
  /* Read the shared physical counter only if an owner was actually active. */
  UINT64 ExitTick = mBqOwner.active ? ArmReadCntPct() : 0;
  (VOID)a04_fw_exit(&mBqOwner, ExitTick, mBqRecord, 128);
  WriteBackDataCacheRange(mBqRecord, 128);
  MemoryFence();
}
STATIC EFI_STATUS SetupBqTransport(VOID)
{
  EFI_ACPI_TABLE_PROTOCOL *Acpi;
  EFI_ACPI_SDT_PROTOCOL *Sdt;
  EFI_ACPI_SDT_HEADER *Found;
  EFI_ACPI_TABLE_VERSION Version;
  EFI_PHYSICAL_ADDRESS Address = MAX_UINT32;
  EFI_STATUS Status;
  UINT8 Table[sizeof(mBqSsdtTemplate)], Sum = 0;
  UINTN Index, Key;
  /* The AML scopes BAT0: installing its SSDT first makes namespace loading
   * fail. Do not install unless the board battery SSDT is already present. */
  Status = gBS->LocateProtocol(&gEfiAcpiSdtProtocolGuid, NULL, (VOID **)&Sdt);
  if (EFI_ERROR(Status)) return Status;
  for (Index = 0; Index < 256; ++Index) {
    Status = Sdt->GetAcpiTable(Index, &Found, &Version, &Key);
    if (EFI_ERROR(Status)) return Status;
    if (Found && Found->Length >= sizeof(*Found) &&
        Found->Signature == SIGNATURE_32('S','S','D','T') &&
        CompareMem(&Found->OemTableId, "A04BATT ", 8) == 0) break;
  }
  if (Index == 256) return EFI_NOT_FOUND;
  Status = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&Acpi);
  if (EFI_ERROR(Status)) return Status;
  Status = gBS->AllocatePages(AllocateMaxAddress, EfiACPIMemoryNVS, 1, &Address);
  if (EFI_ERROR(Status)) return Status;
  mBqRecord = (UINT8 *)(UINTN)Address;
  ZeroMem(mBqRecord, EFI_PAGE_SIZE);
  mBqOwner.last_result = A04_BQ_STOPPED;
  Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_NOTIFY, BqExit, NULL,
                              &gEfiEventExitBootServicesGuid, &mBqExit);
  if (EFI_ERROR(Status)) goto FailPages;
  Status = gBS->CreateEvent(EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_NOTIFY, BqTimer, NULL, &mBqTimer);
  if (EFI_ERROR(Status)) goto FailExit;
  Status = gBS->SetTimer(mBqTimer, TimerPeriodic, 100000000ULL);
  if (EFI_ERROR(Status)) goto FailTimer;
  CopyMem(Table, mBqSsdtTemplate, sizeof(Table));
  Table[A04_BQ_ADDRESS_OFFSET] = (UINT8)Address;
  Table[A04_BQ_ADDRESS_OFFSET+1] = (UINT8)(Address >> 8);
  Table[A04_BQ_ADDRESS_OFFSET+2] = (UINT8)(Address >> 16);
  Table[A04_BQ_ADDRESS_OFFSET+3] = (UINT8)(Address >> 24);
  Table[9] = 0;
  for (Index = 0; Index < sizeof(Table); ++Index) Sum = (UINT8)(Sum + Table[Index]);
  Table[9] = (UINT8)(0 - Sum);
  /* InstallAcpiTable copies the AML; only its NVS data is updated at EBS. */
  Status = Acpi->InstallAcpiTable(Acpi, Table, sizeof(Table), &Key);
  if (!EFI_ERROR(Status)) return EFI_SUCCESS;
  (VOID)gBS->SetTimer(mBqTimer, TimerCancel, 0);
FailTimer:
  (VOID)gBS->CloseEvent(mBqTimer); mBqTimer = NULL;
FailExit:
  (VOID)gBS->CloseEvent(mBqExit); mBqExit = NULL;
FailPages:
  (VOID)gBS->FreePages(Address, 1); mBqRecord = NULL;
  return Status;
}
STATIC VOID EFIAPI BqReady(EFI_EVENT Event, VOID *Context)
{
  EFI_STATUS Status;
  (VOID)Context;
  (VOID)gBS->CloseEvent(Event);
  Status = SetupBqTransport();
  if (EFI_ERROR(Status)) DEBUG((DEBUG_ERROR,"A04BAT BQ transport unavailable=%r\n",Status));
  else BqTryStart();
}
#endif
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
#if A04BR3_BQ_HANDOFF
static UINT32 BqPinMode(UINT32 Pin)
{ return (MmioRead32(GPIO + 0x300 + (Pin / 5) * 16) >> ((Pin % 5) * 3)) & 7; }
STATIC BOOLEAN BqHardwareReady(VOID)
{
  UINT32 Dir0 = MmioRead32(GPIO), Dir1 = MmioRead32(GPIO + 0x10);
  UINT32 Out0 = MmioRead32(GPIO + 0x100), In0 = MmioRead32(GPIO + 0x200);
  if ((MmioRead32(TOP + 0x24) & BIT16) || (MmioRead32(INFRA + 0x80) & BIT3) ||
      BqPinMode(60) != 1 || BqPinMode(61) != 1 || BqPinMode(9) ||
      !(Dir0 & BIT9) || (Out0 & BIT9) || (In0 & BIT9) ||
      BqPinMode(34) || BqPinMode(35) || (Dir1 & 12)) return FALSE;
  return !(MmioRead16(I2C + 0x24) & 1) && MmioRead16(I2C + 0x20) &&
         !(MmioRead16(I2C + 0x48) & 1) && MmioRead16(I2C + 0x40) == 3 &&
         MmioRead16(I2C + 0x28) == 0x8001 && MmioRead16(I2C + 0x60) == 0;
}
static uint16_t BqRead16(void *Context, unsigned Offset)
{ (VOID)Context; return MmioRead16(I2C + Offset); }
static void BqWrite16(void *Context, unsigned Offset, uint16_t Value)
{ (VOID)Context; MmioWrite16(I2C + Offset, Value); MemoryFence(); }
static uint64_t BqNow(void *Context)
{ (VOID)Context; return GetTimeInNanoSecond(GetPerformanceCounter()); }
static uint64_t BqPhysical(void *Context)
{ (VOID)Context; return ArmReadCntPct(); }
static void BqDelay(void *Context, unsigned Us)
{ (VOID)Context; MicroSecondDelay(Us); }
/* No runtime/registry/build flag may substitute for actual evidence.  This
 * must remain FALSE until first-owner deadline, exclusive I2C ownership,
 * Windows receipt, D0-off continuity and bus-failure recovery are proved. */
static BOOLEAN BqAdmissionVerified(UINT64 *FirstDeadlineNs)
{ (VOID)FirstDeadlineNs; return FALSE; }
STATIC VOID BqTryStart(VOID)
{
  UINT64 Deadline = 0, First, Last, PerfFrequency, PhysicalFrequency;
  EFI_TPL Old;
  struct a04_fw_io Io;
  if (!BqAdmissionVerified(&Deadline)) {
    mBqOwner.bq.stopped = 1;
    return;
  }
  /* TPL serializes timer, EBS and initialization on the firmware dispatcher;
   * cross-CPU and other I2C2 owners remain part of the unfulfilled proof. */
  Old = gBS->RaiseTPL(TPL_NOTIFY);
  PerfFrequency = GetPerformanceCounterProperties(&First, &Last);
  PhysicalFrequency = ArmReadCntFrq();
  if (!mBqTimer || !mBqExit || !mBqRecord || !Deadline ||
      !PerfFrequency || PerfFrequency != PhysicalFrequency || First != 0 || Last != MAX_UINT64 ||
      EFI_ERROR(ConfigureI2c2()) || !BqHardwareReady()) {
    mBqOwner.bq.stopped = 1; gBS->RestoreTPL(Old); return;
  }
  mBqPio.context = NULL; mBqPio.read16 = BqRead16; mBqPio.write16 = BqWrite16;
  mBqPio.now_ns = BqNow; mBqPio.delay_us = BqDelay;
  Io.context = &mBqPio; Io.now_ns = BqNow; Io.physical_counter = BqPhysical;
  Io.read = a04_bq_pio_read; Io.write_watchdog = a04_bq_pio_kick;
  if (!a04_fw_start(&mBqOwner, &Io, PhysicalFrequency, Deadline, 1, 1, 1, 1))
    (VOID)a04_fw_tick(&mBqOwner);
  gBS->RestoreTPL(Old);
}
#endif
STATIC VOID EFIAPI BeforeExit(EFI_EVENT Event, VOID *Context)
{
  EFI_STATUS Status;
#if A04BR3_BQ_HANDOFF
  EFI_TPL Old = gBS->RaiseTPL(TPL_NOTIFY);
  if (mBqOwner.active || mBqOwner.bq.attempts || mBqPio.faulted) {
    gBS->RestoreTPL(Old); return;
  }
#endif
  Status=ConfigureI2c2();
#if A04BR3_BQ_HANDOFF
  gBS->RestoreTPL(Old);
#endif
  DEBUG((DEBUG_ERROR,"A04BAT UEFI I2C2 setup=%r\n",Status));
}
EFI_STATUS EFIAPI I2c2HandoffEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;
  EFI_STATUS Status;
  Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,BeforeExit,NULL,
                             &gEfiEventBeforeExitBootServicesGuid,&Event);
  if (EFI_ERROR(Status)) return Status;
#if A04BR3_BQ_HANDOFF
  Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, BqReady, NULL,
                              &gEfiEventReadyToBootGuid, &Event);
  if (EFI_ERROR(Status)) DEBUG((DEBUG_ERROR,"A04BAT BQ ready event unavailable=%r\n",Status));
#endif
  /* The existing I2C2 setup event remains usable if optional transport fails. */
  return EFI_SUCCESS;
}
