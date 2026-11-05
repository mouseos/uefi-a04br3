// Pi.c: Entry point for SEC(Security).

#include <PiPei.h>
#include <Pi/PiBootMode.h>

#include <Library/PrePiLib.h>
#include <Library/PrintLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/PlatformSecLib.h>
#include <Library/BaseLib.h>
#include <Library/SerialPortLib.h>
#if defined (MDE_CPU_ARM)
#include <Library/A04BR3TestCycleLib.h>
#endif

#include <Ppi/GuidedSectionExtraction.h>

#include "Pi.h"
#if A04BR3_THERMAL_PROVIDER == 1
#include <libfdt.h>
#include <Library/HobLib.h>
#include <A04BR3ThermalCal.h>
STATIC EFI_GUID mThermalCalGuid = A04BR3_THERMAL_CAL_GUID;
STATIC VOID CaptureThermalCal (UINT32 Address) {
  CONST VOID *Tree; CONST UINT8 *Prop; UINT32 Count, Size; INT32 Node, Len; A04BR3_THERMAL_CAL Cal;
  if (Address < 0x40000000U || Address > 0xbfffffd8U || (Address & 3U)) return;
  Tree=(CONST VOID *)(UINTN)Address;
  if (fdt_check_header(Tree) != 0) return;
  Size=fdt_totalsize(Tree);
  if (Size < 40 || Size > 0x100000U || Size > 0xc0000000U-Address) return;
  Node=fdt_path_offset(Tree,"/chosen"); if (Node<0) Node=fdt_path_offset(Tree,"/chosen@0");
  if (Node<0) return;
  Prop=(CONST UINT8 *)fdt_getprop(Tree,Node,"atag,devinfo",&Len);
  if (Prop==NULL || Len<256) return;
  Count=ReadUnaligned32((CONST UINT32 *)Prop);
  if (Count<63 || Count>300 || Count>(UINT32)(Len-4)/4) return;
  Cal.Version=1;
  Cal.Devinfo60=ReadUnaligned32((CONST UINT32 *)(Prop+4+60*4));
  Cal.Devinfo61=ReadUnaligned32((CONST UINT32 *)(Prop+4+61*4));
  Cal.Devinfo62=ReadUnaligned32((CONST UINT32 *)(Prop+4+62*4));
  if (BuildGuidDataHob(&mThermalCalGuid,&Cal,sizeof(Cal))==NULL) DEBUG ((DEBUG_ERROR,"A04 THERMAL: calibration HOB unavailable\n"));
  else DEBUG ((DEBUG_ERROR,"A04 THERMAL: boot devinfo captured\n"));
}
#endif
#if A04BR3_WINDOWS_CPU_1300 == 1
INT32 A04BR3WindowsCpu1300 (VOID);
#include <A04BR3CpuHandoff.h>
#include <Library/HobLib.h>
STATIC EFI_GUID mCpuHandoffGuid = A04BR3_CPU_HANDOFF_GUID;
STATIC UINT32 mCpuHandoffValue;
#endif

#define IS_XIP()  (((UINT64)FixedPcdGet64 (PcdFdBaseAddress) > mSystemMemoryEnd) ||\
                  ((FixedPcdGet64 (PcdFdBaseAddress) + FixedPcdGet32 (PcdFdSize)) <= FixedPcdGet64 (PcdSystemMemoryBase)))

UINT64  mSystemMemoryEnd = FixedPcdGet64 (PcdSystemMemoryBase) +
                           FixedPcdGet64 (PcdSystemMemorySize) - 1;

VOID EFIAPI ProcessLibraryConstructorList(VOID);

volatile UINT32* wdt_reg = (volatile UINT32*)0x10007000;

VOID DisableWDT()
{
	*wdt_reg = 0x22000000;
}

VOID
PrePiMain(
  IN VOID *StackBase,
  IN UINTN StackSize,
  IN UINT32 BootDtb
  )
{

  EFI_HOB_HANDOFF_INFO_TABLE *HobList;
  EFI_STATUS                  Status;

  UINTN MemoryBase     = 0;
  UINTN MemorySize     = 0;
  UINTN UefiMemoryBase = 0;
  UINTN UefiMemorySize = 0;

  // Architecture-specific initialization
  // Enable Floating Point
  ArmEnableVFP();

#if defined (MDE_CPU_AARCH64)
  if (ArmReadCurrentEL() == AARCH64_EL2) {
    // Trap General Exceptions. All exceptions that would be routed to EL1 are routed to EL2
    ArmWriteHcr(ARM_HCR_TGE);

    /* Enable Timer access for non-secure EL1 and EL0
       The cnthctl_el2 register bits are architecturally
       UNKNOWN on reset.
       Disable event stream as it is not in use at this stage
    */
    ArmWriteCntHctl(CNTHCTL_EL2_EL1PCTEN | CNTHCTL_EL2_EL1PCEN);
  }

#endif

  /* Enable program flow prediction, if supported */
  ArmEnableBranchPrediction();

  // Declare UEFI region
  MemoryBase     = FixedPcdGet32(PcdSystemMemoryBase);
  MemorySize     = FixedPcdGet32(PcdSystemMemorySize);
  UefiMemoryBase = FixedPcdGet32(PcdA04WinHobBase);
  UefiMemorySize = FixedPcdGet32(PcdA04WinHobSize);
  // The actual SP is established by ModuleEntryPoint from the same stack
  // PCDs. Never substitute a different address for the HOB boundary.
  if (UefiMemorySize < StackSize || UefiMemoryBase == 0 ||
      UefiMemoryBase + UefiMemorySize < UefiMemoryBase ||
      (UINTN)StackBase != UefiMemoryBase + UefiMemorySize - StackSize) {
    DEBUG ((DEBUG_ERROR, "A04 ARM32: inconsistent HOB/stack layout\n"));
    CpuDeadLoop ();
  }

  DEBUG(
      (EFI_D_INFO | EFI_D_LOAD,
       "UEFI Memory Base = 0x%llx, Size = 0x%llx \n"
       "Stack Base = 0x%llx, Stack Size = 0x%llx \n",
       UefiMemoryBase, UefiMemorySize, StackBase, StackSize));

  // Set up HOB
  HobList = HobConstructor(
      (VOID *)UefiMemoryBase, UefiMemorySize, (VOID *)UefiMemoryBase,
      StackBase);

  PrePeiSetHobList (HobList);
#if A04BR3_WINDOWS_CPU_1300 == 1 && A04BR3_A64_WDT_TEST == 1
  if (mCpuHandoffValue == A04BR3_CPU_HANDOFF_COMPLETE) {
    if (BuildGuidDataHob (&mCpuHandoffGuid, &mCpuHandoffValue, sizeof(mCpuHandoffValue)) == NULL) {
      DEBUG ((DEBUG_ERROR, "A04 CPU1300 handoff HOB allocation failed\n"));
    }
  }
#endif

  // Invalidate cache
  InvalidateDataCacheRange(
      (VOID *)(UINTN)PcdGet64(PcdFdBaseAddress), PcdGet32(PcdFdSize));

#if defined (MDE_CPU_ARM)
  {
    UINT32 Sctlr;
    __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (Sctlr));
    DEBUG ((DEBUG_ERROR, "A04REG before MMU SCTLR=%08x CNTFRQ=%u\n", Sctlr, (UINT32)ArmReadCntFrq ()));
    // LK entered in AArch64: normalize AArch32 short-descriptor controls.
    // TRE/AFE alter TEX and AP interpretation; WXN/UWXN prohibit writable code.
    Sctlr &= ~(BIT29 | BIT28 | BIT20 | BIT19);
    __asm__ volatile ("mcr p15, 0, %0, c1, c0, 0; isb" :: "r" (Sctlr) : "memory");
    DEBUG ((DEBUG_ERROR, "A04REG normalized SCTLR=%08x\n", Sctlr));
  }
#endif
#if A04BR3_CACHE_TEST == 1 && defined (MDE_CPU_ARM)
  // ArmLib requires MMU disabled for whole-cache set/way invalidation.
  DEBUG ((DEBUG_ERROR, "A04CACHE invalidate data before MMU\n"));
  ArmInvalidateDataCache ();
#endif
  // Initialize MMU
  Status = MemoryPeim(UefiMemoryBase, UefiMemorySize);
  ASSERT_EFI_ERROR (Status);
#if A04BR3_THERMAL_PROVIDER == 1
  CaptureThermalCal (BootDtb);
#else
  (VOID)BootDtb;
#endif

  // Add HOBs
  BuildStackHob ((UINTN)StackBase, StackSize);

  // TODO: Call CpuPei as a library
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdA04WinCpuIoSize));

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_DEFAULT_SETTINGS);

  // Initialize Platform HOBs (CpuHob and FvHob)
  DEBUG((EFI_D_INFO, "PlatformPeim In \n"));
  Status = PlatformPeim();
  ASSERT_EFI_ERROR (Status);

  // Now, the HOB List has been initialized, we can register performance information
  // PERF_START (NULL, "PEI", NULL, StartTimeStamp);

  // SEC phase needs to run library constructors by hand.
  ProcessLibraryConstructorList();

  // Assume the FV that contains the SEC (our code) also contains a compressed FV.
  DEBUG((EFI_D_INFO, "DecompressFirstFv In \n"));
  Status = DecompressFirstFv();
  ASSERT_EFI_ERROR (Status);

  // Load the DXE Core and transfer control to it
  DEBUG((EFI_D_INFO, "LoadDxeCoreFromFv In \n"));
  Status = LoadDxeCoreFromFv(NULL, 0);
  ASSERT_EFI_ERROR (Status);
}

#if defined (MDE_CPU_ARM)
VOID EFIAPI A04EarlyFault (UINT32 Type, UINT32 Lr, UINT32 SavedPsr)
{
  UINT32 Dfsr, Dfar, Ifsr, Ifar, Sctlr;
  __asm__ volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (Dfsr));
  __asm__ volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r" (Dfar));
  __asm__ volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r" (Ifsr));
  __asm__ volatile ("mrc p15, 0, %0, c6, c0, 2" : "=r" (Ifar));
  __asm__ volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (Sctlr));
  DEBUG ((DEBUG_ERROR, "A04FAULT type=%x lr=%08x psr=%08x sctlr=%08x\n", Type, Lr, SavedPsr, Sctlr));
  DEBUG ((DEBUG_ERROR, "A04FAULT dfsr=%08x dfar=%08x ifsr=%08x ifar=%08x\n", Dfsr, Dfar, Ifsr, Ifar));
  for (;;) { CpuPause (); }
}
#endif

VOID SerialPortInit (VOID)
{
  SerialPortInitialize ();

  DEBUG ((EFI_D_INFO, "\nEDK II For MediaTek Project\n"));
  DEBUG (
      (EFI_D_INFO, "Firmware version %s built %a %a\n\n",
       (CHAR16 *)PcdGetPtr(PcdFirmwareVersionString), __TIME__, __DATE__));
}

VOID
CEntryPoint(
  IN VOID *StackBase,
  IN UINTN StackSize,
  IN UINT32 BootDtb
  )
{
  SerialPortInit ();
  // The canonical SEC hook stops the inherited watchdog. Run it before any
  // opt-in test-cycle arm, never after it.
  PlatformInitialize();
#if defined (MDE_CPU_ARM) && A04BR3_BOOT_DIAGNOSTIC == 1
  /* PlatformInitialize disables the inherited watchdog. Arm immediately
   * after it, before DXE can fault and before its CPU1 worker is started. */
  if (!A04BR3SetFastbootFlag ()) {
    DEBUG ((DEBUG_ERROR, "A04DIAG early RTC fastboot flag FAILED\n"));
    CpuDeadLoop ();
  }
  A04BR3WatchdogArm (A04BR3_WATCHDOG_SECONDS);
  if (!A04BR3WatchdogEnabled ()) {
    DEBUG ((DEBUG_ERROR, "A04DIAG early watchdog enable FAILED\n"));
    CpuDeadLoop ();
  }
  DEBUG ((DEBUG_ERROR, "A04DIAG early bounded watchdog armed\n"));
#endif
  // The canonical board SEC hook owns recovery watchdog setup; do not arm
  // a second time here. Its build-time policy controls servicing mode.

  // Goto primary Main.
  PrePiMain(StackBase, StackSize, BootDtb);

  // DXE Core should always load and never return
  ASSERT(FALSE);
}

VOID
SecondaryCEntryPoint(
  IN  UINTN  MpId
  )
{
  // We must never get into this function on UniCore system
  ASSERT(FALSE);
}
