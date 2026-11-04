/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <PiPei.h>
#include <A04ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/A04BR3TestCycleLib.h>
#if A04BR3_WINDOWS_CPU_1300 == 1
#include "Cpu1300Core.h"
static unsigned int ReadMmio(unsigned int a) { return MmioRead32(a); }
static void WriteMmio(unsigned int a, unsigned int v) { MmioWrite32(a,v); ArmDataSynchronizationBarrier(); }
static int ReadPmic(unsigned int a, unsigned int *v) { UINT32 t; if (!A04BR3PmicRead(a,&t)) return 0; *v=t; return 1; }
static int WritePmic(unsigned int a, unsigned int v) { return A04BR3PmicWrite(a,v); }
static int Guard(unsigned int c, unsigned int s, unsigned int *v) { UINT32 t; if (!A04BR3RtcSpare0Update(c,s,&t)) return 0; *v=t; return 1; }
static int Delay(unsigned int us) {
  UINT64 f=ArmReadCntFrq(), start, now, ticks; UINT32 n;
  if (!f) return 0;
  ticks=(f*us+999999U)/1000000U; start=ArmReadCntPct();
  for (n=0;n<10000000U;n++) { now=ArmReadCntPct(); if (now-start>=ticks) return 1; }
  return 0;
}
int A04BR3WindowsCpu1300(void) {
  const CPU1300_IO io={ReadMmio,WriteMmio,ReadPmic,WritePmic,Guard,Delay};
  int result=Cpu1300Transition(&io);
  DEBUG((DEBUG_ERROR,"A04 CPU1300 SEC result=%d (registers only; no thermal control)\n",result));
  if (result==2) { DEBUG((DEBUG_ERROR,"A04 CPU1300 failure: guard retained, waiting for watchdog\n")); CpuDeadLoop(); }
  return result;
}
#endif
