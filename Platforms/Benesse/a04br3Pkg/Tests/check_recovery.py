#!/usr/bin/env python3
"""Execute the actual SEC/DXE recovery C against host event/MMIO substitutes.

This checks control flow, not real PMIC/WDT operation or reset destination.
"""
from pathlib import Path
import re
import subprocess
import tempfile

PKG = Path(__file__).resolve().parents[1]
ROOT = PKG.parents[2]
SOURCES = [
    ROOT / 'Silicon/MediaTek/MT8167Pkg/Library/PlatformSecLib/PlatformSec.c',
    PKG / 'Library/A04BR3PlatformSecBoardLib/A04BR3PlatformSecBoardLib.c',
    PKG / 'Drivers/A04BR3TestCycleDxe/A04BR3TestCycleDxe.c',
]
PREFIX = r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
typedef void VOID; typedef int BOOLEAN; typedef uint64_t UINT64;
typedef uint32_t UINT32; typedef uintptr_t UINTN; typedef unsigned EFI_TPL;
typedef void *EFI_EVENT; typedef void *EFI_HANDLE; typedef void EFI_SYSTEM_TABLE;
typedef unsigned EFI_STATUS; typedef void (*NOTIFY)(EFI_EVENT, VOID *);
#define STATIC static
#define EFIAPI
#define IN
#define TRUE 1
#define FALSE 0
#define MAX_UINT64 UINT64_MAX
#define EFI_SUCCESS 0
#define EFI_ERROR(x) ((x)!=0)
#define TPL_CALLBACK 8
#define TPL_NOTIFY 16
#define EVT_TIMER 1
#define EVT_NOTIFY_SIGNAL 2
#define TimerPeriodic 1
#define A04BR3_WATCHDOG_SECONDS 31
#define WDT_MODE_REGISTER 0x10007000U
#define WDT_MODE_KEY 0x22000000U
static void DebugLog(int level,const char *fmt,...) {(void)level;(void)fmt;}
#define DEBUG_ERROR 1
#define DEBUG(args) DebugLog args
static jmp_buf fault;
static unsigned arms,kicks,writes,flag_calls,deadloops;
static unsigned create_ex,create_timer,set_timer,fail_ex,fail_timer,fail_set;
static BOOLEAN rtc_ok=TRUE,enabled=TRUE,inject_ebs,pending_ebs;
static UINT64 now=1000000,frequency=1000;
static EFI_TPL tpl;
static NOTIFY ebs_callback,timer_callback;
static int gEfiEventExitBootServicesGuid;
static void CpuDeadLoop(void) {deadloops++;longjmp(fault,1);}
static void MmioWrite32(UINTN a,UINT32 v) {
 assert(a==WDT_MODE_REGISTER && v==WDT_MODE_KEY && arms==0); writes++;
}
static BOOLEAN A04BR3SetFastbootFlag(void) {assert(arms>0);flag_calls++;return rtc_ok;}
static void A04BR3WatchdogArm(UINT32 s) {assert(s==31);arms++;enabled=TRUE;}
static void A04BR3WatchdogKick(void) {assert(tpl==TPL_NOTIFY);kicks++;}
static BOOLEAN A04BR3WatchdogEnabled(void) {return enabled;}
static UINT64 GetPerformanceCounter(void) {
 if(inject_ebs) {pending_ebs=TRUE;inject_ebs=FALSE;}
 return now;
}
static UINT64 GetPerformanceCounterProperties(UINT64 *first,UINT64 *last) {
 *first=0;*last=UINT64_MAX;return frequency;
}
static EFI_TPL Raise(EFI_TPL t) {EFI_TPL old=tpl;assert(t>=tpl);tpl=t;return old;}
static void Restore(EFI_TPL old) {
 tpl=old;
 if(pending_ebs && ebs_callback) {
  pending_ebs=FALSE;tpl=TPL_NOTIFY;ebs_callback(NULL,NULL);tpl=old;
 }
}
static EFI_STATUS CreateEx(UINT32 flags,EFI_TPL t,NOTIFY f,VOID *c,const VOID *g,EFI_EVENT *e) {
 (void)flags;(void)c;(void)g;assert(t==TPL_NOTIFY);create_ex++;
 if(fail_ex)return 1;
 *e=(void*)1;ebs_callback=f;return 0;
}
static EFI_STATUS Create(UINT32 flags,EFI_TPL t,NOTIFY f,VOID *c,EFI_EVENT *e) {
 (void)flags;(void)c;assert(t==TPL_CALLBACK && ebs_callback);create_timer++;
 if(fail_timer)return 1;
 *e=(void*)2;timer_callback=f;return 0;
}
static EFI_STATUS Set(EFI_EVENT e,unsigned type,UINT64 period) {
 assert(e==(void*)2 && type==TimerPeriodic && period==10000000 && ebs_callback);
 set_timer++;return fail_set?1:0;
}
static struct {
 EFI_TPL (*RaiseTPL)(EFI_TPL); void (*RestoreTPL)(EFI_TPL);
 EFI_STATUS (*CreateEventEx)(UINT32,EFI_TPL,NOTIFY,VOID*,const VOID*,EFI_EVENT*);
 EFI_STATUS (*CreateEvent)(UINT32,EFI_TPL,NOTIFY,VOID*,EFI_EVENT*);
 EFI_STATUS (*SetTimer)(EFI_EVENT,unsigned,UINT64);
} services={Raise,Restore,CreateEx,Create,Set},*gBS=&services;
void PlatformSecBoardInitialize(void);
'''
SUFFIX = r'''
static void reset(void) {
 arms=kicks=writes=flag_calls=deadloops=0;
 create_ex=create_timer=set_timer=fail_ex=fail_timer=fail_set=0;
 rtc_ok=enabled=TRUE;inject_ebs=pending_ebs=FALSE;
 now=1000000;frequency=1000;tpl=0;ebs_callback=timer_callback=NULL;
 mKicking=mExited=FALSE;mKickEvent=mExitBootServicesEvent=NULL;
}
static void enter(void) {
 if(setjmp(fault)==0) {
  assert(A04BR3TestCycleDxeEntry(NULL,NULL)==EFI_SUCCESS);
  assert(A04BR3_EARLY_FAULT!=2 || fail_ex || fail_timer || fail_set || !frequency || !rtc_ok);
 } else {assert(A04BR3_EARLY_FAULT==2 || !rtc_ok);}
}
int main(void) {
 reset();
 if(setjmp(fault)==0) {PlatformInitialize();assert(A04BR3_EARLY_FAULT!=1);}
 else assert(A04BR3_EARLY_FAULT==1);
 assert(writes==1 && arms==1 && flag_calls==1);
 reset();rtc_ok=FALSE;
 if(setjmp(fault)==0) {PlatformInitialize();} else assert(0);
 assert(writes==1 && arms==1 && flag_calls==1 && deadloops==0);
 reset();enter();assert(mKicking && create_ex==1 && create_timer==1 && set_timer==1);
 now=mStartCounter+119999;OnKickTimer(NULL,NULL);assert(kicks==1);
 now=mStartCounter+120000;enabled=FALSE;unsigned a=arms;
 OnKickTimer(NULL,NULL);assert(!mKicking && kicks==1 && arms==a);
 now+=100000;OnKickTimer(NULL,NULL);assert(kicks==1 && arms==a);
 reset();enter();now=mStartCounter+500000;OnKickTimer(NULL,NULL);assert(kicks==0 && !mKicking);
 reset();now=UINT64_MAX-60000;enter();now=mStartCounter+120000;
 OnKickTimer(NULL,NULL);assert(kicks==0 && !mKicking);
 reset();enter();a=arms;tpl=TPL_NOTIFY;OnExitBootServices(NULL,NULL);tpl=0;
 assert(mExited && !mKicking && arms==a+1 && set_timer==1);
 OnKickTimer(NULL,NULL);assert(kicks==0);
 OnExitBootServices(NULL,NULL);assert(arms==a+1);
 reset();enter();inject_ebs=TRUE;OnKickTimer(NULL,NULL);
 assert(kicks==1 && mExited && !mKicking);OnKickTimer(NULL,NULL);assert(kicks==1);
 reset();fail_ex=1;enter();assert(!mKicking && create_timer==0 && set_timer==0 && arms==1);
 reset();fail_timer=1;enter();assert(!mKicking && set_timer==0 && arms==1);
 OnKickTimer(NULL,NULL);assert(kicks==0);
 reset();fail_set=1;enter();assert(!mKicking && arms==1);OnKickTimer(NULL,NULL);assert(kicks==0);
 reset();frequency=0;enter();assert(!mKicking && create_ex==0 && arms==1);
 reset();rtc_ok=FALSE;enter();assert(!mKicking && create_ex==0 && arms==1 && deadloops==0);
 return 0;
}
'''
body = '\n'.join(re.sub(r'^\s*#include[^\n]*', '', p.read_text(), flags=re.M) for p in SOURCES)
assert 'LocateMemoryRegionByName' not in body.split('VOID\nPlatformInitialize')[0]
with tempfile.TemporaryDirectory(prefix='a04br3-recovery-host-') as tmp:
    src = Path(tmp) / 'recovery.c'
    src.write_text(PREFIX + body + SUFFIX)
    for mode in (0, 1, 2):
        binary = Path(tmp) / f'recovery-{mode}'
        subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-parameter', '-Wno-unused-function',
                        '-fsanitize=address,undefined', f'-DA04BR3_EARLY_FAULT={mode}',
                        str(src), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('PASS actual SEC/DXE C: default/SEC/DXE smoke; arm-before-RTC; elapsed/late/wrap deadline; EBS ordering and timer race; registration failures/no feeding (ASan/UBSan). Hardware reset not tested.')
