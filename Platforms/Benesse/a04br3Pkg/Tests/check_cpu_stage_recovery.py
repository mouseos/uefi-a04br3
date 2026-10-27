#!/usr/bin/env python3
"""Exercise the production SEC hook with mocked WDT, RTC and register reads."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sec = (root / 'Library/A04BR3PlatformSecBoardLib/A04BR3PlatformSecBoardLib.c').read_text()
header = (root / 'Include/A04BR3CpuStage.h').read_text()
dxe = (root / 'Drivers/A04BR3TestCycleDxe/A04BR3TestCycleDxe.c').read_text()
dsc = (root / 'a04br3.dsc').read_text()
assert '!if $(A04BR3_CPU_STAGE_DIAG) == 1 && $(TARGET) != DEBUG' in dsc
assert dxe.index('Flag = A04BR3SetFastbootFlag ();') < dxe.index('if (Flag) {\n    CONST CHAR8 *Stage = "EBS"')
function = sec[sec.index('VOID\nEFIAPI\nPlatformSecBoardInitialize'):]
ebs_start = dxe.index('STATIC\nVOID\nEFIAPI\nOnExitBootServices')
ebs_end = dxe.index('\nEFI_STATUS\nEFIAPI\nA04BR3TestCycleDxeEntry', ebs_start)
ebs = dxe[ebs_start:ebs_end]
harness = r'''
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#define STATIC static
#define EFIAPI
#define VOID void
#define CONST const
#define CHAR8 char
#define BOOLEAN int
#define IN
#define EFI_EVENT void *
#define FALSE 0
#define TRUE 1
#define UINT32 uint32_t
#define UINTN uintptr_t
#define DEBUG_ERROR 1
#define A04BR3_WATCHDOG_SECONDS 31
#define A04BR3_EARLY_FAULT 0
#define DEBUG(x) debug x
static int step, rtc_ok, reads;
static BOOLEAN mKicking, mExited;
static void debug(int level, const char *fmt, ...) { (void)level; (void)fmt; }
static void A04BR3WatchdogArm(unsigned n) { assert(n==31 && step==0); step=1; }
static int A04BR3SetFastbootFlag(void) { assert(step==1); step=2; return rtc_ok; }
static uint32_t MmioRead32(uintptr_t a) { assert(step==2 && rtc_ok); ++reads; return (uint32_t)a; }
''' + header + '\n' + function + '\n' + ebs + r'''
int main(void) {
 rtc_ok=0; step=0; reads=0; PlatformSecBoardInitialize(); assert(step==2 && reads==0);
 rtc_ok=1; step=0; reads=0; PlatformSecBoardInitialize();
#if A04BR3_CPU_STAGE_DIAG == 1
 assert(reads==8);
#else
 assert(reads==0);
#endif
 rtc_ok=0; step=0; reads=0; mExited=0; mKicking=1;
 OnExitBootServices(0,0); assert(!mKicking && mExited && step==2 && reads==0);
 rtc_ok=1; step=0; reads=0; mExited=0; mKicking=1;
 OnExitBootServices(0,0); assert(!mKicking && mExited && step==2);
#if A04BR3_CPU_STAGE_DIAG == 1
 assert(reads==8);
#else
 assert(reads==0);
#endif
 step=0; reads=0; OnExitBootServices(0,0); assert(step==0 && reads==0);
}
'''
with tempfile.TemporaryDirectory() as t:
    p = Path(t)
    (p / 'test.c').write_text(harness)
    for enabled in (0, 1):
        exe = p / f'test{enabled}'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-parameter',
                        f'-DA04BR3_CPU_STAGE_DIAG={enabled}', str(p/'test.c'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print('PASS actual SEC hook: WDT then RTC; RTC failure gives zero diagnostic reads; default off; EBS/DEBUG guards')
