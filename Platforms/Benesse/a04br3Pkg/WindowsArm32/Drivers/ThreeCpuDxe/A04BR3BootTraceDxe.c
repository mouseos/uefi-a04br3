/** Board-specific 3-CPU parking handoff. Based on edk2-mtk b8ca32e.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/IoLib.h>
#include <Library/A04BR3TestCycleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>
#include <IndustryStandard/Acpi50.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/AcpiTable.h>
#include "CpuParkingSsdt.h"
#define A04BR3_FOUR_CPU_TEST 0
#define A04BR3_A64_WDT_TEST 1
#define A04BR3_KERNEL_MARKER 1
#define A04_SMP_CPU_COUNT 3
#define A04_SMP_FIRST_AP 2
#ifndef A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT
#define A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT 0
#endif
#ifndef A04BR3_BOOT_DIAGNOSTIC
#define A04BR3_BOOT_DIAGNOSTIC 0
#endif
extern UINT32 A04RequestA64Probe (UINT32 Context);
extern UINT32 A04RequestParkingProbe (UINT32 Context, UINT32 CpuId);
extern UINT8 A04ParkingStart[], A04ParkingEnd[];
STATIC volatile UINT32 *mProbePage;
STATIC BOOLEAN mThreeCpuReady;
#pragma pack(1)
typedef struct {
  EFI_ACPI_5_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER Header;
  EFI_ACPI_5_0_GIC_STRUCTURE Cpu[A04_SMP_CPU_COUNT];
  EFI_ACPI_5_0_GIC_DISTRIBUTOR_STRUCTURE Distributor;
} A04_SMP_MADT;
typedef struct {
  EFI_ACPI_5_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER Header;
  EFI_ACPI_5_0_GIC_STRUCTURE Cpu;
  EFI_ACPI_5_0_GIC_DISTRIBUTOR_STRUCTURE Distributor;
} A04_SINGLE_MADT;
#pragma pack()

STATIC VOID EFIAPI A04PublishSmp (EFI_EVENT Event, VOID *Context)
{
  EFI_ACPI_SDT_PROTOCOL *Sdt;
  EFI_ACPI_TABLE_PROTOCOL *Tables;
  EFI_ACPI_SDT_HEADER *Found;
  EFI_ACPI_TABLE_VERSION Version;
  EFI_PHYSICAL_ADDRESS Page[A04_SMP_CPU_COUNT - 1];
  A04_SINGLE_MADT Original;
  A04_SMP_MADT New;
  UINTN Index, Key, NewKey, SsdtKey, Cpu, Poll, Length;
  EFI_STATUS Status;
  volatile UINT32 *Mailbox;
  UINT32 Result;
  (VOID)Context;
  gBS->CloseEvent (Event);
  Status = gBS->LocateProtocol (&gEfiAcpiSdtProtocolGuid, NULL, (VOID **)&Sdt);
  if (EFI_ERROR (Status)) { DEBUG ((DEBUG_ERROR, "A04SMP SDT missing %r\n", Status)); CpuDeadLoop (); }
  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&Tables);
  if (EFI_ERROR (Status)) { DEBUG ((DEBUG_ERROR, "A04SMP table protocol missing %r\n", Status)); CpuDeadLoop (); }
  for (Index = 0; ; Index++) {
    Status = Sdt->GetAcpiTable (Index, &Found, &Version, &Key);
    if (EFI_ERROR (Status)) { DEBUG ((DEBUG_ERROR, "A04SMP MADT missing %r\n", Status)); CpuDeadLoop (); }
    if (Found->Signature == EFI_ACPI_5_0_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE) { break; }
  }
  if (Found->Length != sizeof (Original)) { DEBUG ((DEBUG_ERROR, "A04SMP unexpected MADT length=%u\n", Found->Length)); CpuDeadLoop (); }
  CopyMem (&Original, Found, sizeof (Original));
  if (Original.Cpu.Type != EFI_ACPI_5_0_GIC || Original.Cpu.GicId != 0 ||
      Original.Distributor.Type != EFI_ACPI_5_0_GICD) { DEBUG ((DEBUG_ERROR, "A04SMP unexpected topology\n")); CpuDeadLoop (); }
  Length = (UINTN)A04ParkingEnd - (UINTN)A04ParkingStart;
  if (Length == 0 || Length > 0x700) { DEBUG ((DEBUG_ERROR, "A04SMP parking code invalid\n")); CpuDeadLoop (); }
  ZeroMem (&New, sizeof (New));
  New.Header = Original.Header;
  New.Header.Header.Length = sizeof (New);
  New.Cpu[0] = Original.Cpu;
  New.Distributor = Original.Distributor;
  for (Cpu = 0; Cpu < A04_SMP_CPU_COUNT - 1; Cpu++) {
    Page[Cpu] = 0xffffffff;
    Status = gBS->AllocatePages (AllocateMaxAddress, EfiReservedMemoryType, 1, &Page[Cpu]);
    if (EFI_ERROR (Status)) { DEBUG ((DEBUG_ERROR, "A04SMP page cpu%u %r\n", (UINT32)Cpu + A04_SMP_FIRST_AP, Status)); CpuDeadLoop (); }
    Mailbox = (volatile UINT32 *)(UINTN)Page[Cpu];
    ZeroMem ((VOID *)(UINTN)Page[Cpu], 4096);
    Mailbox[0] = MAX_UINT32;
    CopyMem ((VOID *)(UINTN)(Page[Cpu] + 0x800), A04ParkingStart, Length);
    WriteBackInvalidateDataCacheRange ((VOID *)(UINTN)Page[Cpu], 4096);
#if A04BR3_FOUR_CPU_TEST == 1
    if (Cpu != 0) {
#endif
    Result = A04RequestParkingProbe ((UINT32)Page[Cpu], (UINT32)Cpu + A04_SMP_FIRST_AP);
    DEBUG ((DEBUG_ERROR, "A04SMP cpu%u CPU_ON64=%08x\n", (UINT32)Cpu + A04_SMP_FIRST_AP, Result));
    if ((INT32)Result != 0) { CpuDeadLoop (); }
    for (Poll = 0; Poll < 1000; Poll++) {
      InvalidateDataCacheRange ((VOID *)(UINTN)(Page[Cpu] + 0xfc0), 64);
      if (Mailbox[0x3ff] == 0xa04bcafe) { break; }
      gBS->Stall (1000);
    }
    DEBUG ((DEBUG_ERROR, "A04SMP cpu%u parked=%08x\n", (UINT32)Cpu + A04_SMP_FIRST_AP, Mailbox[0x3ff]));
    if (Mailbox[0x3ff] != 0xa04bcafe) { CpuDeadLoop (); }
#if A04BR3_FOUR_CPU_TEST == 1
    }
#endif
    New.Cpu[Cpu + 1] = Original.Cpu;
    New.Cpu[Cpu + 1].GicId = (UINT32)Cpu + A04_SMP_FIRST_AP;
    New.Cpu[Cpu + 1].AcpiProcessorUid = (UINT32)Cpu + A04_SMP_FIRST_AP;
    New.Cpu[Cpu + 1].ParkingProtocolVersion = 1;
    New.Cpu[Cpu + 1].ParkedAddress = Page[Cpu];
  }
  Status = Tables->InstallAcpiTable (Tables, (VOID *)mThreeCpuSsdt, sizeof(mThreeCpuSsdt), &SsdtKey);
  if (EFI_ERROR (Status)) { DEBUG ((DEBUG_ERROR, "A04SMP SSDT install %r\n", Status)); CpuDeadLoop (); }
  Status = Tables->UninstallAcpiTable (Tables, Key);
  if (EFI_ERROR (Status)) { Tables->UninstallAcpiTable (Tables, SsdtKey); DEBUG ((DEBUG_ERROR, "A04SMP remove MADT %r\n", Status)); CpuDeadLoop (); }
  Status = Tables->InstallAcpiTable (Tables, &New, sizeof (New), &NewKey);
  DEBUG ((DEBUG_ERROR, "A04SMP install MADT %r\n", Status));
  if (!EFI_ERROR (Status) && mProbePage != NULL) {
#if A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT != 1
#if A04BR3_FOUR_CPU_TEST == 1
    mProbePage[7] = (UINT32)(Page[1] + 0xffc);
    mProbePage[8] = (UINT32)(Page[2] + 0xffc);
    mProbePage[9] = (UINT32)Page[0];
#else
    mProbePage[7] = (UINT32)(Page[0] + 0xffc);
    mProbePage[8] = (UINT32)(Page[1] + 0xffc);
#endif
    WriteBackDataCacheRange ((VOID *)mProbePage, 64);
    DEBUG ((DEBUG_ERROR, "A04SMP mailbox monitoring armed\n"));
#endif
    mThreeCpuReady = TRUE;
#if A04BR3_BOOT_DIAGNOSTIC == 1
    DEBUG ((DEBUG_ERROR, "A04DIAG CPU0/2/3 tables installed; OS entry blocked; watchdog bounded\n"));
    CpuDeadLoop ();
#endif
  }
  if (EFI_ERROR (Status)) {
    Tables->UninstallAcpiTable (Tables, SsdtKey);
    Status = Tables->InstallAcpiTable (Tables, &Original, sizeof (Original), &NewKey);
    DEBUG ((DEBUG_ERROR, "A04SMP restore MADT %r\n", Status));
    CpuDeadLoop ();
  }
}
STATIC VOID ProbeSecondaryA64 (VOID)
{
  EFI_PHYSICAL_ADDRESS Page = 0xffffffff;
  volatile UINT32 *Marker;
  UINTN Index, Result;
#if A04BR3_A64_WDT_TEST != 1
  UINTN Cpu, Level;
#endif
  EFI_STATUS Status = gBS->AllocatePages (AllocateMaxAddress, EfiReservedMemoryType, 1, &Page);
  if (EFI_ERROR (Status)) { DEBUG ((DEBUG_ERROR, "A04TRACE A64 secondary page %r\n", Status)); CpuDeadLoop (); }
  Marker = (volatile UINT32 *)(UINTN)Page;
#if A04BR3_KERNEL_MARKER == 1
  mProbePage = Marker;
  ZeroMem ((VOID *)Marker, 64);
#endif
  *Marker = 0;
  WriteBackInvalidateDataCacheRange ((VOID *)Marker, 64);
  DEBUG ((DEBUG_ERROR, "A04TRACE A64 HVC enter context=%08x\n", (UINT32)Page));
  Result = A04RequestA64Probe ((UINT32)Page);
  DEBUG ((DEBUG_ERROR, "A04TRACE A64 HVC CPU_ON64 result=%08x\n", (UINT32)Result));
  if ((INT32)Result == 0) {
    for (Index = 0; Index < 1000; Index++) {
      InvalidateDataCacheRange ((VOID *)Marker, 64);
      if (*Marker == 0xa064beef) { break; }
      gBS->Stall (1000);
    }
    DEBUG ((DEBUG_ERROR, "A04TRACE A64 secondary marker=%08x polls=%u\n", *Marker, (UINT32)Index));
    if (*Marker != 0xa064beef) CpuDeadLoop ();
#if A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT == 1
    {
      UINT32 Last, Changes;
      InvalidateDataCacheRange ((VOID *)Marker, 64);
      Last = Marker[11];
      Changes = 0;
      for (Index = 0; Index < 12000 && Changes < 2; Index++) {
        gBS->Stall (1000);
        InvalidateDataCacheRange ((VOID *)Marker, 64);
        if (Marker[11] != Last) { Last = Marker[11]; Changes++; }
      }
      if (Changes < 2) { DEBUG ((DEBUG_ERROR, "A04TRACE CPU1 feed counter stopped\n")); CpuDeadLoop (); }
      DEBUG ((DEBUG_ERROR, "A04TRACE CPU1 feed writes changed twice; TOPRGU mode=%08x\n", MmioRead32 (0x10007000)));
#if A04BR3_BOUNDED_RECOVERY == 1
      /* PrePi armed this before entering DXE; verify RTC and re-arm once
       * the CPU1 feed worker has demonstrated two writes. */
      if (!A04BR3SetFastbootFlag ()) { DEBUG ((DEBUG_ERROR, "A04DIAG RTC fastboot flag FAILED\n")); CpuDeadLoop (); }
      A04BR3WatchdogArm (A04BR3_WATCHDOG_SECONDS);
      if (!A04BR3WatchdogEnabled ()) { DEBUG ((DEBUG_ERROR, "A04DIAG watchdog enable FAILED\n")); CpuDeadLoop (); }
      InvalidateDataCacheRange ((VOID *)Marker, 64);
      Last = Marker[11]; Changes = 0;
      for (Index = 0; Index < 12000 && Changes < 2; Index++) {
        gBS->Stall (1000);
        InvalidateDataCacheRange ((VOID *)Marker, 64);
        if (Marker[11] != Last) { Last = Marker[11]; Changes++; }
      }
      if (Changes < 2 || !A04BR3WatchdogEnabled ()) { DEBUG ((DEBUG_ERROR, "A04DIAG armed watchdog/worker FAILED\n")); CpuDeadLoop (); }
      DEBUG ((DEBUG_ERROR, "A04DIAG watchdog enabled, fastboot RTC verified, two subsequent feed writes\n"));
#endif
    }
#endif
#if A04BR3_A64_WDT_TEST == 1
    DEBUG ((DEBUG_ERROR, "A04TRACE CPU1 entry observed; watchdog state and continued feeding not verified\n"));
#else
    if (*Marker == 0xa064beef) {
      gBS->Stall (10000);
      Cpu = 1; Level = 0;
      Result = ArmCallSmc2 (0x84000004, &Cpu, &Level, NULL);
      DEBUG ((DEBUG_ERROR, "A04TRACE A64 cpu1 after=%08x\n", (UINT32)Result));
    }
#endif
  }
  if ((INT32)Result != 0) CpuDeadLoop ();
  // Reserved page is retained until reset, even if the worker stalls.
}
STATIC VOID EFIAPI A04BeforeExit(EFI_EVENT Event, VOID *Context)
{
  (VOID)Event; (VOID)Context;
  DEBUG ((DEBUG_ERROR, "A04TRACE ExitBootServices event signaled; completion not yet known\n"));
#if A04BR3_BOOT_DIAGNOSTIC == 1
  DEBUG ((DEBUG_ERROR, "A04DIAG ExitBootServices blocked\n"));
  CpuDeadLoop ();
#endif
  if (!mThreeCpuReady) CpuDeadLoop ();
}
EFI_STATUS EFIAPI A04BR3BootTraceEntry(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT BeforeExit, Ready; EFI_STATUS Status;
  (VOID)ImageHandle; (VOID)SystemTable;
  mThreeCpuReady = FALSE;
  Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_NOTIFY, A04BeforeExit, NULL,
                              &gEfiEventBeforeExitBootServicesGuid, &BeforeExit);
  if (EFI_ERROR(Status)) CpuDeadLoop ();
  ProbeSecondaryA64();
  Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, A04PublishSmp, NULL,
                              &gEfiEventReadyToBootGuid, &Ready);
  if (EFI_ERROR(Status)) CpuDeadLoop ();
  return EFI_SUCCESS;
}
