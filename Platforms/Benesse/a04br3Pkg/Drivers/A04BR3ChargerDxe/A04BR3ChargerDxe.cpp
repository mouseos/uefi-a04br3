// SPDX-License-Identifier: MIT
// Board-only, default OFF (module absent). Reads start the BQ watchdog and may
// consume FAULT history. Only preserve the matching LK policy; never repair it.
extern "C" {
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/EventGroup.h>
}
#define A04CHARGER_UEFI_TYPES
using uint8_t = UINT8;
using uint16_t = UINT16;
using uint64_t = UINT64;
#include <A04BR3ChargerCore.h>
#include <A04BR3ChargerBoardCore.h>
#include <A04BR3ChargerHandoff.h>
#include <DarwinBootBoardHandoff.h>

static EFI_EVENT mTimer, mExitEvent;
static BOOLEAN mArmed, mExited;
static UINT64 mFrequency;
static UINT64 mLastPhysical;
static uint8_t *mHandoff;
static unsigned mLastResult=a04charger::Stopped;
static UINT64 PhysicalCounter() {
#ifdef A04CHARGER_HOST_TEST
  return GetPerformanceCounter();
#else
  UINT64 v; __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v)); return v;
#endif
}
static UINT64 PhysicalFrequency() {
#ifdef A04CHARGER_HOST_TEST
  return mFrequency;
#else
  UINT64 v; __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v;
#endif
}
static a04charger::Owner mOwner;

struct Ops {
  bool updateMux(unsigned mask, unsigned bits) {
    // All firmware board initialization here runs at TPL_NOTIFY.
    unsigned v=MmioRead32(0x10001080); MmioWrite32(0x10001080,(v&~mask)|bits);
    MemoryFence(); return (MmioRead32(0x10001080)&mask)==bits;
  }
  unsigned rd32(unsigned a) { return MmioRead32(a); }
  void wr32(unsigned a, unsigned v) { MmioWrite32(a,v); MemoryFence(); }
  UINT16 rd(unsigned o) { return MmioRead16(0x1100b000 + o); }
  void wr(unsigned o, UINT16 v) { MmioWrite16(0x1100b000 + o, v); MemoryFence(); }
  UINT64 now() { return GetTimeInNanoSecond(GetPerformanceCounter()); }
  void delay() { MicroSecondDelay(5); }
  bool read(unsigned reg, UINT8 *v) { return a04charger::transfer(*this, true, UINT8(reg), v); }
  bool kick(UINT8 v) { bool ok=a04charger::transfer(*this, false, 0, &v); if(ok)mLastPhysical=PhysicalCounter(); return ok; }
};
static unsigned Mode(unsigned pin) {
  return (MmioRead32(0x10005000 + 0x300 + (pin / 5) * 16) >> ((pin % 5) * 3)) & 7;
}
static bool Ready() {
  UINT32 gate = MmioRead32(0x10000024), dir = MmioRead32(0x10005000);
  UINT32 out = MmioRead32(0x10005100), in = MmioRead32(0x10005200);
  UINT32 dir1 = MmioRead32(0x10005010);
  // Check inherited clocks/pins only; no clock enable/reset, GPIO or PWRAP writes.
  if ((gate & (1U << 16)) || Mode(60) != 1 || Mode(61) != 1 || Mode(9) ||
      !(dir & (1U << 9)) || (out & (1U << 9)) || (in & (1U << 9)) ||
      Mode(34) || Mode(35) || (dir1 & 12)) return false;
  Ops io;
  return !(io.rd(0x24) & 1) && io.rd(0x20) && !(io.rd(0x48) & 1) &&
         io.rd(0x40) == 3 && io.rd(0x60) == 0;
}
static VOID EFIAPI Tick(EFI_EVENT Event, VOID *Context) {
  (void)Event; (void)Context;
  // Serialize the complete MMIO transaction against EBS. No resumed callback
  // can touch the charger after EBS sets mExited at TPL_NOTIFY.
  EFI_TPL old = gBS->RaiseTPL(TPL_NOTIFY);
  if (!mArmed || mExited || mOwner.stopped) { gBS->RestoreTPL(old); return; }
  Ops ops;
  a04charger::Result result;
  if (!Ready()) { mOwner.stopped = true; result = a04charger::Stopped; }
  else result = a04charger::step(mOwner, ops);
  mLastResult=unsigned(result);
  const auto &s = mOwner.latest;
  DEBUG((DEBUG_ERROR, "A04BR3 charger: t=%Lu valid=%02x raw=%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x r=%u kicks=%u firstFAULT=%u history=%02x\n",
         s.at, s.valid, s.raw[0], s.raw[1], s.raw[2], s.raw[3], s.raw[4], s.raw[5], s.raw[6], s.raw[7],
         unsigned(result), mOwner.kicks, (mOwner.first.valid & 1) ? mOwner.first.raw[0] & 7 : 255, mOwner.faultHistory));
  if (result != a04charger::Maintained) {
    mArmed = FALSE;
    DEBUG((DEBUG_ERROR, "A04BR3 charger: stopped; no repair/retry; 30 s fallback may change policy\n"));
  }
  gBS->RestoreTPL(old);
}
static VOID EFIAPI Exit(EFI_EVENT Event, VOID *Context) {
  (void)Event; (void)Context;
  if(mExited)return;
  bool active=mArmed;
  mExited = TRUE;
  mArmed = FALSE;
  if(mHandoff) {a04charger::publish(mHandoff,mOwner,active,mLastPhysical,PhysicalFrequency(),PhysicalCounter(),mLastResult); MemoryFence();}
  // No charger read/write, timer API, allocation, or final unvalidated pet here.
  // Kernel must explicitly opt in and establish its owner before remaining WDT
  // time elapses. This callback cannot promise that runtime deadline is met.
  Ops ops;
  DEBUG((DEBUG_ERROR, "A04BR3 charger: EBS stopped; kicks=%u last=%Lu now=%Lu ns; kernel owner required\n",
         mOwner.kicks, mOwner.lastKick, ops.now()));
}
static EFI_STATUS EFIAPI AttachTree(VOID *tree, UINTN bytes) {
  if(!tree&&!bytes&&!mExited){mHandoff=NULL;return EFI_SUCCESS;} // loader failure detaches before freeing its window
  if(!tree||bytes>0xffffffffULL||mHandoff||mExited)return EFI_UNSUPPORTED;
  auto slot=a04charger::findSlot(static_cast<uint8_t *>(tree),unsigned(bytes));
  if(!slot)return EFI_UNSUPPORTED;
  for(unsigned i=0;i<a04charger::HandoffBytes;++i)if(slot[i])return EFI_UNSUPPORTED;
  mHandoff=slot;return EFI_SUCCESS;
}
static DARWIN_BOOT_BOARD_HANDOFF mHook={1,AttachTree};
extern "C" EFI_STATUS EFIAPI A04BR3ChargerDxeEntry(EFI_HANDLE Image, EFI_SYSTEM_TABLE *SystemTable) {
  (void)Image; (void)SystemTable;
  UINT64 first, last;
  mFrequency = GetPerformanceCounterProperties(&first, &last);
  // Same up-counting, full-width monotonic contract as the recovery module.
  // Bound conversion horizon to >=1 day; unsupported counters do no BQ access.
  if (!mFrequency || first != 0 || last != MAX_UINT64 ||
      GetPerformanceCounter() / mFrequency >= 86400) return EFI_UNSUPPORTED;
  EFI_GUID hookGuid=DARWIN_BOOT_BOARD_HANDOFF_GUID;
  EFI_HANDLE handle=NULL;
  // Establish every callback before publishing pointers into this driver image.
  // Any EFI_ERROR return below leaves neither events nor a discoverable protocol:
  // the dispatcher may unload the failed image immediately.
  EFI_STATUS status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_NOTIFY, Exit, NULL,
                                        &gEfiEventExitBootServicesGuid, &mExitEvent);
  if (EFI_ERROR(status)) return status;
  status = gBS->CreateEvent(EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Tick, NULL, &mTimer);
  if (EFI_ERROR(status)) { gBS->CloseEvent(mExitEvent); mExitEvent=NULL; return status; }
  status = gBS->SetTimer(mTimer, TimerPeriodic, 100000000ULL); // 10 s, 100 ns units
  if (EFI_ERROR(status)) {
    gBS->CloseEvent(mTimer); gBS->CloseEvent(mExitEvent);
    mTimer=mExitEvent=NULL; return status;
  }
  // Last fallible operation: success keeps the driver resident; no EFI_ERROR
  // path is permitted after publication. Initial hardware/policy refusal returns
  // EFI_SUCCESS with a stopped owner, so its callbacks remain valid.
  status=gBS->InstallMultipleProtocolInterfaces(&handle,&hookGuid,&mHook,NULL);
  if (EFI_ERROR(status)) {
    gBS->CloseEvent(mTimer); gBS->CloseEvent(mExitEvent);
    mTimer=mExitEvent=NULL; return status;
  }
  // Record inherited controller state before dedicated-resource initialization.
  DEBUG((DEBUG_ERROR, "A04BR3 charger: inherited gate=%08x mux=%08x pins=%08x timing=%04x div=%04x\n",
         MmioRead32(0x10000024), MmioRead32(0x10001080), MmioRead32(0x100053c0),
         (MmioRead32(0x10000024) & (1U<<16)) ? 0xffff : MmioRead16(0x1100b020),
         (MmioRead32(0x10000024) & (1U<<16)) ? 0xffff : MmioRead16(0x1100b070)));
  DEBUG((DEBUG_ERROR, "A04BR3 charger: GPIO dir0=%08x out0=%08x in0=%08x dir1=%08x in1=%08x; gated timing=ffff unavailable\n",
         MmioRead32(0x10005000), MmioRead32(0x10005100), MmioRead32(0x10005200),
         MmioRead32(0x10005010), MmioRead32(0x10005210)));
  DEBUG((DEBUG_ERROR, "A04BR3 charger: modes EN%u USB%u AC%u SDA%u SCL%u ahb=%08x maincon=%08x pcw=%08x pwr=%08x\n",
         Mode(9), Mode(34), Mode(35), Mode(60), Mode(61), MmioRead32(0x10000000),
         MmioRead32(0x10018120), MmioRead32(0x10018124), MmioRead32(0x10018130)));
  EFI_TPL old = gBS->RaiseTPL(TPL_NOTIFY);
  Ops io; auto setup = a04charger::initialize(io);
  DEBUG((DEBUG_ERROR, "A04BR3 charger: I2C2 initialize result=%u (0 ready)\n", unsigned(setup)));
  mArmed = setup == a04charger::BoardReady;
  if (!mArmed) mOwner.stopped=true;
  gBS->RestoreTPL(old);
  // Timer and EBS ownership exist before the first BQ read.
  Tick(NULL, NULL);
  return EFI_SUCCESS;
}
