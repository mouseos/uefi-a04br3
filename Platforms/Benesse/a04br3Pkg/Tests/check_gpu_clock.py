#!/usr/bin/env python3
"""Host execution of the actual DXE callback with mocked MMIO/PWRAP/variables."""
from pathlib import Path
import os, subprocess, tempfile
pkg = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
    tmp=Path(tmp)
    for name in ('Library/DebugLib.h','Library/IoLib.h','Library/TimerLib.h',
                 'Library/UefiBootServicesTableLib.h','Library/UefiRuntimeServicesTableLib.h',
                 'Guid/EventGroup.h'):
        f=tmp/name; f.parent.mkdir(parents=True,exist_ok=True); f.write_text('')
    (tmp/'Uefi.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <wchar.h>
typedef uint32_t UINT32; typedef uint16_t UINT16; typedef uint64_t UINT64;
typedef size_t UINTN; typedef wchar_t CHAR16; typedef int BOOLEAN;
typedef void VOID; typedef void *EFI_EVENT; typedef void *EFI_HANDLE;
typedef void EFI_SYSTEM_TABLE; typedef int EFI_STATUS;
typedef struct {UINT32 a; UINT16 b,c; unsigned char d[8];} EFI_GUID;
#define STATIC static
#define CONST const
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define BIT21 (1U<<21)
#define BIT25 (1U<<25)
#define EFI_SUCCESS 0
#define EFI_NOT_FOUND 1
#define EFI_TIMEOUT 2
#define EFI_ERROR(x) ((x)!=0)
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 2
#define EFI_VARIABLE_RUNTIME_ACCESS 4
#define EVT_NOTIFY_SIGNAL 1
#define TPL_CALLBACK 1
#define DEBUG_ERROR 1
static void Debug(int level,const char *fmt,...) {(void)level;(void)fmt;}
#define DEBUG(x) Debug x
#define STATIC_ASSERT _Static_assert
static UINT32 MmioRead32(UINTN);
static void MmioWrite32(UINTN,UINT32);
static void MicroSecondDelay(UINTN);
static EFI_STATUS SetVar(CHAR16*,EFI_GUID*,UINT32,UINTN,VOID*);
static EFI_STATUS EventEx(int,int,void(*)(EFI_EVENT,VOID*),VOID*,EFI_GUID*,EFI_EVENT*);
static struct {EFI_STATUS (*SetVariable)(CHAR16*,EFI_GUID*,UINT32,UINTN,VOID*);} rt={SetVar},*gRT=&rt;
static struct {EFI_STATUS (*CreateEventEx)(int,int,void(*)(EFI_EVENT,VOID*),VOID*,EFI_GUID*,EFI_EVENT*);} bs={EventEx},*gBS=&bs;
static EFI_GUID gEfiEventBeforeExitBootServicesGuid;
''')
    harness=r'''
#include "DRIVER"
static UINT32 mux,slow,sel,con0,con1,power,rails[4],fsm,pmic;
static int published,fail_read,fail_publish,fail_power,fail_mux,change,reads;
static MT8167_GPU_CLOCK_RECORD saved;
static UINT32 MmioRead32(UINTN a) {
  switch(a) {
    case 0x10000000: return fail_mux ? (4U<<8) : mux;
    case 0x10000040: return slow;
    case 0x10000004: return sel;
    case 0x10018160: return con0;
    case 0x10018164: return con1;
    case 0x10018170: return power;
    case WACS_DATA: return BIT21 | (fsm<<16) | pmic;
    case 0x1000660c: case 0x10006610: return fail_power?0:(1U<<25)|(1U<<24)|(1U<<4);
    case 0x10200634: return BIT25;
    default:return 0;
  }
}
static void MmioWrite32(UINTN a,UINT32 v) {
  assert(a!=0x10018160 && a!=0x10018164 && a!=0x10018170);
  if(a==0x10000000)mux=v;
  if(a==0x10000040)slow=v;
  if(a==0x10000004)sel=v;
  if(a==WACS_CMD) {
    assert(!(v & (1U<<31))); /* PMIC writes forbidden. */
    switch((v>>16)<<1) {
      case 0x030a:pmic=rails[0];break; case 0x030e:pmic=rails[1];break;
      case 0x0312:pmic=rails[2];break; case 0x0314:pmic=rails[3];break;
      default:assert(0);
    }
    if(change && ++reads>4) con1=0x020e0000;
    fsm=fail_read?0:6;
  }
  if(a==WACS_CLEAR)fsm=0;
}
static void MicroSecondDelay(UINTN n) {(void)n;}
static EFI_STATUS SetVar(CHAR16 *n,EFI_GUID *g,UINT32 a,UINTN z,VOID *v) {
  assert(wcscmp(n,L"Mt8167GpuClock")==0);
  EFI_GUID expected=MT8167_GPU_CLOCK_GUID; assert(!memcmp(g,&expected,sizeof(expected)));
  if(z==0){published=0;return EFI_NOT_FOUND;}
  assert(a==6 && z==44);if(fail_publish)return EFI_TIMEOUT;
  saved=*(MT8167_GPU_CLOCK_RECORD*)v;published=1;return 0;
}
static EFI_STATUS EventEx(int a,int b,void(*c)(EFI_EVENT,VOID*),VOID*d,EFI_GUID*e,EFI_EVENT*f)
{(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;}
static void reset(void) {
  mux=slow=sel=fsm=0;con0=0x11;con1=0x020e9d8a;power=5;
  rails[0]=2;rails[1]=0x3001;rails[2]=rails[3]=0x4e;
  published=fail_read=fail_publish=fail_power=fail_mux=change=reads=Attempted=0;
  assert(GpuPowerEntry(NULL,NULL)==0);
}
static void run(void){GpuBeforeExit(NULL,NULL);}
int main(void) {
  reset();run();assert(published==A04BR3_GPU_MMPLL);
  assert(((mux>>8)&7)==(A04BR3_GPU_MMPLL?7:4));
  if(published){assert(saved.version==1 && saved.bytes==44 && saved.parent==7 && saved.clock_hz==380000062);}
  for(unsigned i=0;i<8;i++) {
    reset();switch(i) {
      case 0:con0=0;break;case 1:power=3;break;case 2:rails[1]=1;break;
      case 3:rails[3]=0x47;break;case 4:con1=0;break;
      case 5:fail_read=1;break;case 6:fail_publish=1;break;case 7:change=1;break;
    }
    run();assert(!published && ((mux>>8)&7)==4);
  }
  reset();fail_power=1;run();assert(!published && ((mux>>8)&7)==4);
  reset();fail_mux=1;run();assert(!published && ((mux>>8)&7)==4);
  for(unsigned selector=0;selector<128;selector++) {
    UINT32 hz=a04_rgx_mmpll_hz(0x11,0x020e9d8a,5,2,0x3001,0,selector);
    assert(hz==((selector>=72 && selector<=96)?380000062U:0));
  }
  return 0;
}
'''.replace('DRIVER',str(pkg/'WindowsArm32/Drivers/GpuPowerDxe/GpuPowerDxe.c'))
    (tmp/'test.c').write_text(harness)
    for flag in (0,1):
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',f'-DA04BR3_GPU_MMPLL={flag}',
                        '-I'+str(tmp),str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
        subprocess.run([str(tmp/'test')],check=True)
        print(f'GPU clock flag={flag}: callback, ABI, voltage policy and fallback passed')
