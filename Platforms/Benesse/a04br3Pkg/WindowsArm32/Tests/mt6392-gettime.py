#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
# Production library logic with bounded host stubs, not real UEFI/MMIO/Windows.
from pathlib import Path
import subprocess,os,argparse
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
r=Path(subprocess.check_output(['git','rev-parse','--show-toplevel'],cwd=Path(__file__).parent,text=True).strip())
src=r/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Library/Mt6392RealTimeClockLib';o=Path(a.output).resolve();o.mkdir(parents=True,exist_ok=True)
(o/'tmp').mkdir(exist_ok=True)
body='\n'.join(x for x in (src/'Mt6392Rtc.c').read_text().splitlines() if not x.startswith('#include'))
(o/'runtime-body.inc').write_text(body)
head=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t UINT8;typedef uint16_t UINT16;typedef uint32_t UINT32;typedef uint64_t UINT64;typedef size_t UINTN;typedef int BOOLEAN;typedef void VOID;typedef void* EFI_HANDLE;typedef int EFI_STATUS;typedef unsigned EFI_TPL;typedef struct {int unused;} EFI_SYSTEM_TABLE;
#define STATIC static
#define CONST const
#define IN
#define OUT
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define MAX_UINT32 UINT32_MAX
#define MAX_UINT64 UINT64_MAX
#define EFI_SUCCESS 0
#define EFI_DEVICE_ERROR 1
#define EFI_INVALID_PARAMETER 2
#define EFI_UNSUPPORTED 3
#define EFI_NOT_READY 4
#define EFI_ALREADY_STARTED 5
#define EFI_TIMEOUT 6
#define EFI_ERROR(x) ((x)!=0)
#define TPL_HIGH_LEVEL 31
#define EFI_UNSPECIFIED_TIMEZONE 2047
#define ZeroMem(p,n) memset(p,0,n)
#define DivU64x32(a,b) ((a)/(b))
#define DEBUG(x) ((void)0)
#define DEBUG_ERROR 1
typedef struct {UINT16 Year;UINT8 Month,Day,Hour,Minute,Second,Pad1;UINT32 Nanosecond;int16_t TimeZone;UINT8 Daylight,Pad2;} EFI_TIME;
typedef struct {UINT32 Resolution,Accuracy;BOOLEAN SetsToZero;} EFI_TIME_CAPABILITIES;
static unsigned probe_registrations;void MtRtcRegisterBootProbe(void){++probe_registrations;}
static UINT64 counter=1000000;static UINT32 frequency=1000;static unsigned reads,mmio,raised,restored;static int runtime_phase,fault,unstable;
static UINT16 raw[7]={58,59,23,28,0,2,58};
static unsigned synchronized;
void ArmInstructionSynchronizationBarrier(void){synchronized=1;}
UINT64 ArmGenericTimerGetSystemCount(void){assert(synchronized);synchronized=0;return counter;}
UINTN ArmGenericTimerGetTimerFreq(void){return frequency;}
void ArmDataSynchronizationBarrier(void){}
UINT32 MmioRead32(UINTN a){assert(!runtime_phase&&a==0x1000f09c);++mmio;return 1;}
EFI_STATUS ReadPmic(UINT16 a,UINT16*v){assert(!runtime_phase);++reads;++counter;if(fault)return EFI_TIMEOUT;
 if(a==0x8000){*v=0;return 0;}
 assert(a>=0x800a&&a<=0x8016&&!(a&1));*v=raw[(a-0x800a)/2];
 if(unstable&&a==0x800a&&reads%10==9)*v=(UINT16)((*v+1)%60);return 0;}
EFI_TPL raise(EFI_TPL t){assert(t==31);++raised;return 4;}
void restore(EFI_TPL t){assert(t==4);++restored;}
static struct {EFI_TPL(*RaiseTPL)(EFI_TPL);void(*RestoreTPL)(EFI_TPL);} bs={raise,restore},*gBS=&bs;
#include "Calendar.h"
#include "runtime-body.inc"
int main(void){EFI_SYSTEM_TABLE st={0};EFI_TIME t,old;EFI_TIME_CAPABILITIES cap,capold;unsigned before;
 assert(LibRtcInitialize(NULL,&st)==0&&mValid&&raised==1&&restored==1);before=reads;assert(reads==10&&mmio==1);
 runtime_phase=1;counter=mSeedCounter+2*frequency;assert(LibGetTime(&t,NULL)==0);assert(t.Year==2026&&t.Month==3&&t.Day==1&&t.Second==0&&t.TimeZone==2047&&t.Nanosecond==0);
 old=t;memset(&cap,0xa5,sizeof(cap));capold=cap;assert(LibGetTime(&t,&cap)==EFI_UNSUPPORTED&&!memcmp(&cap,&capold,sizeof(cap))&&!memcmp(&t,&old,sizeof(t)));
 assert(LibGetTime(NULL,NULL)==EFI_INVALID_PARAMETER);
 ++frequency;assert(LibGetTime(&t,NULL)==EFI_DEVICE_ERROR);--frequency;
 counter=mSeedCounter-1;assert(LibGetTime(&t,NULL)==EFI_DEVICE_ERROR);
 assert(LibSetTime(&old)==EFI_UNSUPPORTED&&LibSetTime(NULL)==EFI_INVALID_PARAMETER);
 assert(LibSetWakeupTime(FALSE,NULL)==EFI_UNSUPPORTED);
 assert(reads==before&&mmio==1); /* after boot: no PMIC/MMIO, including errors */
 runtime_phase=0;mInitialized=mValid=0;fault=1;counter=1000000;
 assert(LibRtcInitialize(NULL,&st)==EFI_SUCCESS&&!mValid);runtime_phase=1;assert(LibGetTime(&t,NULL)==EFI_DEVICE_ERROR);
 runtime_phase=0;mInitialized=mValid=0;fault=0;unstable=1;reads=0;counter=1000000;
 assert(LibRtcInitialize(NULL,&st)==0&&!mValid&&reads==30);runtime_phase=1;assert(LibGetTime(&t,NULL)==EFI_DEVICE_ERROR);
 assert(raised==3&&restored==3&&probe_registrations==3);puts("PASS actual library control flow: one bounded boot seed, no runtime PMIC/MMIO, frequency/backward/failure rejection, unsupported quality/set/alarm, TPL restored");
}
'''
(o/'runtime-test.c').write_text(head)
env=dict(os.environ,TMPDIR=str(o/'tmp'))
subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I'+str(src),'-I'+str(o),str(o/'runtime-test.c'),'-o',str(o/'runtime-test')],check=True,env=env)
subprocess.run([str(o/'runtime-test')],check=True,env=env)

subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(src),str(r/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Tests/mt6392-calendar-test.c'),'-o',str(o/'calendar-test')],check=True,env=env)
subprocess.run([str(o/'calendar-test')],check=True,env=env)
