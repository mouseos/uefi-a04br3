#!/usr/bin/env python3
"""Execute the actual RTC library and shared bounded PWRAP transport on host."""
from pathlib import Path
import os, subprocess, tempfile
pkg=Path(__file__).resolve().parents[1]
source=pkg/'WindowsArm32/Library/Mt6392RtcLib/Mt6392RtcLib.c'
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as directory:
    tmp=Path(directory)
    for name in ('BaseMemoryLib','DebugLib','IoLib','TimerLib','RealTimeClockLib','UefiBootServicesTableLib','UefiLib','UefiRuntimeLib','ArmLib'):
        p=tmp/'Library'/f'{name}.h';p.parent.mkdir(exist_ok=True);p.write_text('')
    (tmp/'Guid').mkdir();(tmp/'Guid/EventGroup.h').write_text('');(tmp/'Guid/RtPropertiesTable.h').write_text('')
    (tmp/'Uefi.h').write_text(r'''#pragma once
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t UINT8;typedef uint16_t UINT16;typedef uint32_t UINT32;typedef uint64_t UINT64;typedef size_t UINTN;
typedef void VOID;typedef int BOOLEAN;typedef void *EFI_HANDLE;typedef void EFI_SYSTEM_TABLE;typedef int EFI_STATUS;typedef unsigned EFI_TPL;
typedef struct {UINT16 Year;UINT8 Month,Day,Hour,Minute,Second,Pad1;UINT32 Nanosecond;int16_t TimeZone;UINT8 Daylight,Pad2;} EFI_TIME;
typedef struct {UINT32 Resolution,Accuracy;BOOLEAN SetsToZero;} EFI_TIME_CAPABILITIES;
typedef struct {UINT16 Version,Length;UINT32 RuntimeServicesSupported;} EFI_RT_PROPERTIES_TABLE;
typedef int EFI_GUID;
#define STATIC static
#define CONST const
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define BIT21 (1U<<21)
#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 1
#define EFI_TIMEOUT 2
#define EFI_NOT_READY 3
#define EFI_DEVICE_ERROR 4
#define EFI_INVALID_PARAMETER 5
#define EFI_UNSUPPORTED 6
#define EFI_ERROR(x) ((x)!=0)
#define EFI_UNSPECIFIED_TIMEZONE 2047
#define EFI_RT_PROPERTIES_TABLE_VERSION 1
#define EFI_RT_SUPPORTED_GET_TIME 1
#define EFI_RT_SUPPORTED_SET_TIME 2
#define EFI_RT_SUPPORTED_GET_WAKEUP_TIME 4
#define EFI_RT_SUPPORTED_SET_WAKEUP_TIME 8
#define TPL_HIGH_LEVEL 31
#define DEBUG_ERROR 1
#define ZeroMem(p,n) memset(p,0,n)
#define CopyMem(p,q,n) memcpy(p,q,n)
static int runtime,raised,raise_count,restore_count,logs;
static UINT32 fsm,pmic,bbpu,raw[7],commands,sec_reads,delays;
static int initialized=1,stall,rollover,existing,install_fail;
static EFI_RT_PROPERTIES_TABLE previous,*installed;
typedef void *EFI_EVENT;
#define EVT_NOTIFY_SIGNAL 1
#define TPL_CALLBACK 8
static EFI_GUID gEfiRtPropertiesTableGuid,gEfiEventBeforeExitBootServicesGuid;
static void (*before_exit)(EFI_EVENT,VOID*);
static EFI_STATUS EventEx(unsigned type,EFI_TPL tpl,void(*fn)(EFI_EVENT,VOID*),VOID*ctx,EFI_GUID*guid,EFI_EVENT*out){assert(!runtime && type==1 && tpl==8 && !ctx && guid==&gEfiEventBeforeExitBootServicesGuid);before_exit=fn;*out=(void*)1;return 0;}
static EFI_STATUS CloseEvent(EFI_EVENT e){assert(!runtime && e==(void*)1);before_exit=0;return 0;}
#if A04BR3_MT6392_RTC_RUNTIME_COUNTER
static UINT64 counter_ticks;static UINTN counter_freq=13000000;
static UINT64 ArmReadCntPct(void){return counter_ticks;}
static UINTN ArmReadCntFrq(void){return counter_freq;}
#endif
static BOOLEAN EfiAtRuntime(void){return runtime;}
static EFI_TPL Raise(EFI_TPL t){assert(!runtime && !raised && t==31);raised=1;raise_count++;return 0;}
static VOID Restore(EFI_TPL t){assert(!runtime && raised && t==0);raised=0;restore_count++;}
static EFI_STATUS Install(EFI_GUID *g,VOID *v){assert(!runtime && g==&gEfiRtPropertiesTableGuid);if(install_fail)return EFI_DEVICE_ERROR;installed=v;return 0;}
static EFI_STATUS EfiGetSystemConfigurationTable(EFI_GUID*g,VOID**v){assert(!runtime && g==&gEfiRtPropertiesTableGuid);if(!existing)return EFI_NOT_FOUND;*v=&previous;return 0;}
static struct {EFI_TPL(*RaiseTPL)(EFI_TPL);VOID(*RestoreTPL)(EFI_TPL);EFI_STATUS(*InstallConfigurationTable)(EFI_GUID*,VOID*);EFI_STATUS(*CreateEventEx)(unsigned,EFI_TPL,void(*)(EFI_EVENT,VOID*),VOID*,EFI_GUID*,EFI_EVENT*);EFI_STATUS(*CloseEvent)(EFI_EVENT);} bs={Raise,Restore,Install,EventEx,CloseEvent},*gBS=&bs;
static VOID Debug(int level,const char *format,...){assert(!runtime && !raised && level==DEBUG_ERROR);assert(strstr(format,"loss_validity=unknown") || strstr(format,"counter-handoff"));logs++;}
#define DEBUG(x) Debug x
static UINT32 MmioRead32(UINTN a){assert(!runtime && raised && a==0x1000f0a4);return (initialized?BIT21:0)|((stall?2:fsm)<<16)|pmic;}
static VOID MmioWrite32(UINTN a,UINT32 v){assert(!runtime && raised);
 if(a==0x1000f0a8){assert(v==1);fsm=0;return;}
 assert(a==0x1000f0a0 && !(v&(1u<<31)));unsigned address=(v>>16)*2;commands++;fsm=6;
 if(address==0x8000){pmic=bbpu;return;}
 assert(address>=0x800a && address<=0x8016 && !(address&1));unsigned i=(address-0x800a)/2;pmic=raw[i];
 if(i==0){sec_reads++;if(rollover){pmic=(sec_reads&1)?59:0;if(rollover==1 && sec_reads==2){raw[0]=0;raw[3]=1;raw[5]=1;raw[6]=59;rollover=0;}}}
}
static VOID MicroSecondDelay(UINTN n){assert(!runtime && raised && n==1);delays++;}
''')
    harness=r'''
#include "SOURCE"
static EFI_TIME sentinel,answer;
static void reset(void){runtime=raised=raise_count=restore_count=logs=0;fsm=pmic=bbpu=commands=sec_reads=delays=0;initialized=1;stall=rollover=existing=install_fail=0;installed=0;mLogged=0;memset(&previous,0,sizeof(previous));
 raw[0]=23;raw[1]=45;raw[2]=12;raw[3]=7;raw[4]=6;raw[5]=11;raw[6]=58;memset(&sentinel,0xa5,sizeof(sentinel));answer=sentinel;}
static void failure(void){assert(LibGetTime(&answer,0)==EFI_DEVICE_ERROR);assert(!memcmp(&answer,&sentinel,sizeof(answer)));assert(!raised && raise_count==restore_count);}
int main(void){EFI_TIME_CAPABILITIES caps,copy;unsigned saved;
 reset();assert(!LibGetTime(&answer,0));assert(answer.Year==2026 && answer.Month==11 && answer.Day==7 && answer.Hour==12 && answer.Minute==45 && answer.Second==23 && answer.TimeZone==2047 && !answer.Nanosecond && !answer.Daylight && !answer.Pad1 && !answer.Pad2);assert(commands==10 && logs==1);
 assert(!LibGetTime(&answer,0) && logs==1);assert(!raised && raise_count==restore_count);
 reset();raw[6]=46;raw[5]=1;raw[3]=18;assert(!LibGetTime(&answer,0) && answer.Year==2014 && answer.Day==18); /* Never replace a real old date. */
 reset();raw[6]=32;raw[5]=2;raw[3]=29;assert(!LibGetTime(&answer,0) && answer.Year==2000);
 reset();raw[5]=2;raw[3]=29;failure();
 for(unsigned i=0;i<7;i++){if(i==4)continue;reset();raw[i]=(UINT32[]){60,60,24,0,7,13,128}[i];failure();}
 reset();raw[4]=0xffff;assert(!LibGetTime(&answer,0)); /* DOW is not an EFI_TIME field. */
 reset();initialized=0;failure();assert(!commands && !delays);
 reset();stall=1;failure();assert(!commands && delays==10000);
 reset();bbpu=1u<<6;failure();assert(commands==1);
 reset();rollover=1;assert(!LibGetTime(&answer,0) && answer.Year==2027 && answer.Month==1 && answer.Day==1 && answer.Second==0 && commands==19);
 reset();rollover=2;failure();assert(commands==27);
 reset();memset(&caps,0xa5,sizeof(caps));copy=caps;assert(LibGetTime(&answer,&caps)==EFI_UNSUPPORTED && !commands && !raise_count && !memcmp(&caps,&copy,sizeof(caps)) && !memcmp(&answer,&sentinel,sizeof(answer)));
 reset();assert(LibGetTime(0,0)==EFI_INVALID_PARAMETER && !commands && !raise_count);
 reset();runtime=1;assert(LibGetTime(&answer,0)==EFI_UNSUPPORTED && !commands && !raise_count && !memcmp(&answer,&sentinel,sizeof(answer)));assert(LibSetTime(&answer)==EFI_UNSUPPORTED);assert(LibGetWakeupTime(0,0,0)==EFI_INVALID_PARAMETER);assert(LibSetWakeupTime(0,0)==EFI_UNSUPPORTED);
 reset();assert(LibSetTime(&answer)==EFI_UNSUPPORTED && !commands);assert(LibSetTime(0)==EFI_INVALID_PARAMETER);
 reset();assert(!LibRtcInitialize(0,0) && installed && installed->Version==1 && installed->Length==8 && installed->RuntimeServicesSupported==0x3ff0 && logs==1);
 saved=commands;runtime=1;assert(LibGetTime(&answer,0)==EFI_UNSUPPORTED && commands==saved);
 reset();existing=1;previous.Version=1;previous.Length=8;previous.RuntimeServicesSupported=0x291f;assert(!LibRtcInitialize(0,0) && installed->RuntimeServicesSupported==0x2910);
 reset();existing=1;previous.Version=2;previous.Length=8;assert(LibRtcInitialize(0,0)==EFI_UNSUPPORTED && !installed && !commands);
 reset();install_fail=1;assert(LibRtcInitialize(0,0)==EFI_DEVICE_ERROR && !commands);
 reset();initialized=0;assert(!LibRtcInitialize(0,0) && installed && !commands);failure();
 return 0;}
'''.replace('SOURCE',str(source))
    if os.environ.get('RTC_COUNTER_TEST')=='1':
        harness=r'''
#include "SOURCE"
static void setup(void){runtime=raised=raise_count=restore_count=logs=0;fsm=pmic=bbpu=commands=sec_reads=delays=0;initialized=1;stall=rollover=existing=install_fail=0;installed=0;mLogged=0;mCounterValid=0;counter_freq=13000000;counter_ticks=100;gBS=&bs;
 raw[0]=23;raw[1]=45;raw[2]=12;raw[3]=7;raw[4]=6;raw[5]=11;raw[6]=58;}
static void handoff(void){assert(before_exit);before_exit((void*)1,0);runtime=1;gBS=0;}
int main(void){EFI_TIME t,unchanged;unsigned n;
 setup();assert(!LibRtcInitialize(0,0) && installed->RuntimeServicesSupported==0x3ff1);handoff();n=commands;
 counter_ticks+=13000000ULL*61;assert(!LibGetTime(&t,0) && t.Year==2026 && t.Month==11 && t.Day==7 && t.Hour==12 && t.Minute==46 && t.Second==24 && commands==n);
 memset(&unchanged,0xa5,sizeof(unchanged));t=unchanged;counter_freq=14000000;assert(LibGetTime(&t,0)==EFI_DEVICE_ERROR && !memcmp(&t,&unchanged,sizeof(t)));counter_freq=13000000;assert(LibGetTime(&t,0)==EFI_DEVICE_ERROR);
 setup();raw[6]=56;raw[5]=2;raw[3]=28;raw[2]=23;raw[1]=59;raw[0]=59;assert(!LibRtcInitialize(0,0));handoff();counter_ticks+=26000000;assert(!LibGetTime(&t,0) && t.Month==2 && t.Day==29 && t.Hour==0 && t.Minute==0 && t.Second==1);
 setup();raw[6]=46;raw[5]=1;raw[3]=18;assert(!LibRtcInitialize(0,0));handoff();counter_ticks+=13000000ULL*86400*7;assert(!LibGetTime(&t,0) && t.Year==2014 && t.Month==1 && t.Day==25);
 setup();assert(!LibRtcInitialize(0,0));initialized=0;handoff();t=unchanged;assert(LibGetTime(&t,0)==EFI_DEVICE_ERROR && !memcmp(&t,&unchanged,sizeof(t)));
 setup();assert(!LibRtcInitialize(0,0));handoff();counter_ticks--;t=unchanged;assert(LibGetTime(&t,0)==EFI_DEVICE_ERROR && !memcmp(&t,&unchanged,sizeof(t)));
 setup();raw[6]=127;raw[5]=12;raw[3]=31;raw[2]=23;raw[1]=59;raw[0]=59;assert(!LibRtcInitialize(0,0));handoff();counter_ticks+=13000000;assert(LibGetTime(&t,0)==EFI_DEVICE_ERROR);
 setup();assert(!LibRtcInitialize(0,0));handoff();EFI_TIME_CAPABILITIES caps;assert(LibGetTime(&t,&caps)==EFI_UNSUPPORTED && LibSetTime(&t)==EFI_UNSUPPORTED);
 setup();install_fail=1;assert(LibRtcInitialize(0,0)==EFI_DEVICE_ERROR && !before_exit);
 return 0;}
'''.replace('SOURCE',str(source))
    (tmp/'test.c').write_text(harness)
    binary=tmp/'test'
    subprocess.run([os.environ.get('CC','clang'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DA04BR3_MT6392_RTC_RUNTIME_COUNTER='+('1' if os.environ.get('RTC_COUNTER_TEST')=='1' else '0'),'-I'+str(tmp),str(tmp/'test.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS RTC-seeded physical-counter S0 runtime branch with gBS=NULL and forbidden runtime MMIO (ASan/UBSan).' if os.environ.get('RTC_COUNTER_TEST')=='1' else 'PASS actual MT6392 boot library + bounded PWRAP/calendar/rollover/failure tests (ASan/UBSan).')
