#!/usr/bin/env python3
"""Execute actual I2c2HandoffDxe callbacks using boot-services and ACPI fakes."""
from pathlib import Path
import subprocess
import tempfile

pkg=Path(__file__).resolve().parents[2]
dxe=pkg/'Drivers/I2c2HandoffDxe'
mock=r'''
#ifndef MOCK_UEFI_H
#define MOCK_UEFI_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef void VOID; typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef uint64_t UINT64; typedef uintptr_t UINTN; typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef unsigned EFI_STATUS; typedef unsigned EFI_TPL; typedef int BOOLEAN; typedef void *EFI_HANDLE; typedef void *EFI_EVENT; typedef struct { int x; } EFI_SYSTEM_TABLE; typedef struct { int x; } EFI_GUID;
#define STATIC static
#define EFIAPI
#define FALSE 0
#define TRUE 1
#define MAX_UINT64 UINT64_MAX
#define EFI_SUCCESS 0u
#define EFI_UNSUPPORTED 2u
#define EFI_NOT_READY 3u
#define EFI_DEVICE_ERROR 4u
#define EFI_NOT_FOUND 5u
#define SIGNATURE_32(a,b,c,d) ((UINT32)(a)|((UINT32)(b)<<8)|((UINT32)(c)<<16)|((UINT32)(d)<<24))
#define EFI_ERROR(x) ((x)!=0)
#define EVT_NOTIFY_SIGNAL 1u
#define EVT_TIMER 2u
#define TPL_CALLBACK 8u
#define TPL_NOTIFY 16u
#define AllocateMaxAddress 1
#define EfiACPIMemoryNVS 10
#define EFI_PAGE_SIZE 4096u
#define MAX_UINT32 UINT32_MAX
#define TimerPeriodic 1
#define TimerCancel 0
#define BIT16 (1u<<16)
#define BIT3 (1u<<3)
#define BIT9 (1u<<9)
#define DEBUG_ERROR 1
#define DEBUG(x) mock_debug x
static inline void mock_debug(int level, ...){(void)level;}
#define ZeroMem(p,n) memset((p),0,(n))
#define CopyMem(d,s,n) memcpy((d),(s),(n))
#define CompareMem(a,b,n) memcmp((a),(b),(n))
typedef void (*EVENT_FN)(EFI_EVENT,VOID*);
typedef struct { UINT32 Signature,Length; UINT8 filler[8]; UINT64 OemTableId; } EFI_ACPI_SDT_HEADER;
typedef int EFI_ACPI_TABLE_VERSION;
typedef struct { EFI_STATUS (*GetAcpiTable)(UINTN, EFI_ACPI_SDT_HEADER **, EFI_ACPI_TABLE_VERSION *, UINTN *); } EFI_ACPI_SDT_PROTOCOL;
typedef struct acpi_protocol { EFI_STATUS (*InstallAcpiTable)(struct acpi_protocol *,void *,UINTN,UINTN *); } EFI_ACPI_TABLE_PROTOCOL;
typedef struct { EFI_STATUS (*LocateProtocol)(EFI_GUID*,void*,void**); EFI_STATUS (*AllocatePages)(int,int,UINTN,EFI_PHYSICAL_ADDRESS*); EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS,UINTN); EFI_STATUS (*CreateEventEx)(unsigned,unsigned,EVENT_FN,void*,EFI_GUID*,EFI_EVENT*); EFI_STATUS (*CreateEvent)(unsigned,unsigned,EVENT_FN,void*,EFI_EVENT*); EFI_STATUS (*SetTimer)(EFI_EVENT,int,UINT64); EFI_STATUS (*CloseEvent)(EFI_EVENT); EFI_TPL (*RaiseTPL)(EFI_TPL); void (*RestoreTPL)(EFI_TPL); } BOOT_SERVICES;
extern BOOT_SERVICES *gBS; extern EFI_GUID gEfiAcpiTableProtocolGuid,gEfiEventExitBootServicesGuid,gEfiEventBeforeExitBootServicesGuid,gEfiEventReadyToBootGuid,gEfiAcpiSdtProtocolGuid;
UINT32 MmioRead32(UINTN); UINT16 MmioRead16(UINTN); void MmioWrite32(UINTN,UINT32); void MmioWrite16(UINTN,UINT16);
static inline UINT64 RShiftU64(UINT64 x,UINTN n){return x>>n;}
static inline UINT64 MultU64x32(UINT64 x,UINT32 n){return x*n;}
static inline UINT64 DivU64x32(UINT64 x,UINT32 n){return x/n;}
void MemoryFence(void); void *WriteBackDataCacheRange(void *,UINTN);
static inline UINT64 ArmReadCntPct(void){return 0;}
static inline UINT64 ArmReadCntFrq(void){return 13000000;}
static inline UINT64 GetPerformanceCounter(void){return 0;}
static inline UINT64 GetPerformanceCounterProperties(UINT64 *a,UINT64 *b){*a=0;*b=MAX_UINT64;return 13000000;}
static inline UINT64 GetTimeInNanoSecond(UINT64 x){return x;}
static inline UINTN MicroSecondDelay(UINTN x){return x;}
#endif
'''
harness=r'''
#include <assert.h>
#include "I2c2HandoffDxe.c"
static unsigned char page[EFI_PAGE_SIZE], table[256]; static unsigned installed, closes, frees, mmio, flushed; static int install_fail, missing_battery;
EFI_GUID gEfiAcpiTableProtocolGuid,gEfiEventExitBootServicesGuid,gEfiEventBeforeExitBootServicesGuid,gEfiEventReadyToBootGuid,gEfiAcpiSdtProtocolGuid;
static EFI_STATUS install(EFI_ACPI_TABLE_PROTOCOL *a,void *p,UINTN n,UINTN *key){(void)a;assert(n<sizeof(table));if(install_fail)return 1;memcpy(table,p,n);installed=(unsigned)n;*key=1;return 0;}
static EFI_ACPI_TABLE_PROTOCOL acpi={install};
static EFI_STATUS gettable(UINTN i,EFI_ACPI_SDT_HEADER **h,EFI_ACPI_TABLE_VERSION *v,UINTN *k){
 static EFI_ACPI_SDT_HEADER battery; (void)v;(void)k;if(i||missing_battery)return EFI_NOT_FOUND;
 memset(&battery,0,sizeof(battery));battery.Signature=SIGNATURE_32('S','S','D','T');battery.Length=sizeof(battery);
 memcpy(&battery.OemTableId,"A04BATT ",8);*h=&battery;return 0;}
static EFI_ACPI_SDT_PROTOCOL sdt={gettable};
static EFI_STATUS locate(EFI_GUID *g,void *r,void **p){(void)r;*p=(g==&gEfiAcpiSdtProtocolGuid)?(void*)&sdt:(void*)&acpi;return 0;}
static EFI_STATUS alloc(int t,int m,UINTN n,EFI_PHYSICAL_ADDRESS *a){assert(t==AllocateMaxAddress&&m==EfiACPIMemoryNVS&&n==1);*a=(uintptr_t)page;return 0;}
static EFI_STATUS freepages(EFI_PHYSICAL_ADDRESS a,UINTN n){(void)a;(void)n;++frees;return 0;}
static EFI_STATUS eventex(unsigned t,unsigned l,EVENT_FN fn,void *c,EFI_GUID *g,EFI_EVENT *e){(void)t;(void)l;(void)fn;(void)c;(void)g;*e=(void*)1;return 0;}
static EFI_STATUS event(unsigned t,unsigned l,EVENT_FN fn,void *c,EFI_EVENT *e){(void)t;(void)l;(void)fn;(void)c;*e=(void*)2;return 0;}
static EFI_STATUS timer(EFI_EVENT e,int mode,UINT64 n){(void)e;if(mode==TimerPeriodic)assert(n==100000000ULL);return 0;}
static EFI_STATUS closeevent(EFI_EVENT e){(void)e;++closes;return 0;}
static EFI_TPL raisetpl(EFI_TPL t){return t;} static void restoretpl(EFI_TPL t){(void)t;}
static BOOT_SERVICES bs={locate,alloc,freepages,eventex,event,timer,closeevent,raisetpl,restoretpl}; BOOT_SERVICES *gBS=&bs;
UINT32 MmioRead32(UINTN x){(void)x;++mmio;return 0;} UINT16 MmioRead16(UINTN x){(void)x;++mmio;return 0;}
void MmioWrite32(UINTN x,UINT32 y){(void)x;(void)y;++mmio;} void MmioWrite16(UINTN x,UINT16 y){(void)x;(void)y;++mmio;}
void MemoryFence(void){} void *WriteBackDataCacheRange(void *p,UINTN n){assert(p==page&&n==128);++flushed;return p;}
int main(void){unsigned i,sum=0;struct a04_bq_owner os={0};
 assert(!I2c2HandoffEntry(NULL,NULL)); assert(!installed);
 BqReady((void*)3,NULL); assert(installed==sizeof(mBqSsdtTemplate));
 for(i=0;i<installed;++i)sum=(sum+table[i])&255;assert(sum==0);
 assert(!memcmp(table+A04_BQ_ADDRESS_OFFSET,&(UINT32){(UINT32)(uintptr_t)page},4));
 for(i=0;i<4;++i)BqTimer(NULL,NULL);assert(!mmio&&!mBqOwner.bq.attempts);
 BqExit(NULL,NULL);assert(flushed==1&&page[16]==2&&mBqOwner.exited);
 assert(a04_bq_adopt_handoff(&os,page,128,1,1,13000000));
 BqTimer(NULL,NULL);BqExit(NULL,NULL);assert(!mmio&&mBqOwner.exited);
 memset(&mBqOwner,0,sizeof(mBqOwner));mBqTimer=NULL;mBqExit=NULL;mBqRecord=NULL;install_fail=1;
 assert(SetupBqTransport()!=0&&closes==3&&frees==1&&mBqRecord==NULL&&!mmio);
 missing_battery=1;install_fail=0;assert(SetupBqTransport()!=0&&frees==1&&closes==3&&!mmio);
 return 0;}
'''
with tempfile.TemporaryDirectory(prefix='a04-bq-dxe-') as td:
    tmp=Path(td); (tmp/'Uefi.h').write_text(mock)
    for path in ('Library/BaseLib.h','Library/DebugLib.h','Library/IoLib.h','Library/UefiBootServicesTableLib.h','Guid/EventGroup.h','Protocol/AcpiTable.h','Protocol/AcpiSystemDescriptionTable.h','Library/BaseMemoryLib.h','Library/CacheMaintenanceLib.h','Library/ArmLib.h','Library/TimerLib.h'):
        p=tmp/path; p.parent.mkdir(parents=True,exist_ok=True); p.write_text('#include <Uefi.h>\n')
    (tmp/'test.c').write_text(harness)
    lib=pkg/'Library/Bq24160HandoffLib'
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-parameter', '-DA04BR3_BQ_HANDOFF=1', '-I'+str(tmp),'-I'+str(pkg/'Include'),'-I'+str(dxe),str(tmp/'test.c'),str(lib/'charger.c'),str(lib/'BqFirmwareOwner.c'),str(lib/'BqPio.c'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('Actual DXE stopped record, timer, EBS and cleanup: PASS (host fakes)')
