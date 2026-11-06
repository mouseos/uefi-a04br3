#!/usr/bin/env python3
"""Compile the real DXE source with a synthetic UEFI dispatcher."""
import os, pathlib, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[2]
source = root/'Drivers/ThermalHandoffDxe/ThermalHandoffDxe.c'
with tempfile.TemporaryDirectory(prefix='thermal-handoff-', dir=os.environ['TMPDIR']) as td:
 p=pathlib.Path(td);(p/'Library').mkdir();(p/'Guid').mkdir()
 (p/'Uefi.h').write_text('''#include <stdint.h>\n#include <stddef.h>\ntypedef uint32_t UINT32; typedef uint64_t EFI_STATUS; typedef void VOID; typedef void *EFI_EVENT; typedef void *EFI_HANDLE; typedef void EFI_SYSTEM_TABLE; typedef unsigned short CHAR16; typedef struct { uint32_t a;uint16_t b,c;uint8_t d[8]; } EFI_GUID;\n#define STATIC static\n#define CONST const\n#define EFIAPI\n#define EFI_NOT_FOUND 14\n#define EFI_ERROR(x) ((x)!=0)\n#define EFI_VARIABLE_BOOTSERVICE_ACCESS 2\n#define EFI_VARIABLE_RUNTIME_ACCESS 4\n#define EVT_NOTIFY_SIGNAL 0x100\n#define TPL_CALLBACK 8\ntypedef struct { EFI_STATUS (*SetVariable)(CHAR16*,EFI_GUID*,UINT32,unsigned long,void*); } RUNTIME;\ntypedef struct { EFI_STATUS (*CreateEventEx)(int,int,void (*)(EFI_EVENT,void*),void*,EFI_GUID*,EFI_EVENT*); } BOOT;\nextern RUNTIME *gRT;extern BOOT *gBS;extern EFI_GUID gEfiEventBeforeExitBootServicesGuid;\n''')
 (p/'Guid/EventGroup.h').write_text('')
 for name in ['UefiBootServicesTableLib','UefiRuntimeServicesTableLib','UefiDriverEntryPoint']:(p/'Library'/f'{name}.h').write_text('')
 (p/'Library/DebugLib.h').write_text('#define DEBUG_ERROR 1\n#define DEBUG(x) do {} while (0)\n')
 (p/'Library/IoLib.h').write_text('extern uint32_t mock_mmio(uint32_t);\n#define MmioRead32 mock_mmio\n')
 test=p/'test.c';test.write_text('''#include <assert.h>\n#include <string.h>\n#include "SOURCE"\nstatic unsigned calls,create,attrs,reads;static uint32_t clock_value=0x40000002;static A04_THERMAL_HANDOFF_RECORD got;static A04_ADC_OBSERVATION adc;static EFI_STATUS del=EFI_NOT_FOUND;\nuint32_t mock_mmio(uint32_t a){assert(a==0x10000024);reads++;return clock_value;}\nstatic EFI_STATUS set(CHAR16 *n,EFI_GUID *g,UINT32 a,unsigned long size,void *v){(void)n;(void)g;calls++;if(!size)return del;attrs=a;if(size==sizeof(got))memcpy(&got,v,size);else {assert(size==sizeof(adc));memcpy(&adc,v,size);}return 0;}\nstatic EFI_STATUS event(int t,int l,void(*fn)(EFI_EVENT,void*),void*c,EFI_GUID*g,EFI_EVENT*e){(void)t;(void)l;(void)c;(void)g;(void)e;create++;fn(0,0);return 0;}\nstatic RUNTIME rt={set};static BOOT bs={event};RUNTIME *gRT=&rt;BOOT *gBS=&bs;EFI_GUID gEfiEventBeforeExitBootServicesGuid;\nint main(void){assert(!ThermalHandoffEntry(0,0));assert(calls==4&&create==1&&attrs==6&&reads==2);assert(got.version==1&&got.bytes==16&&got.observation==1&&!got.ownership);assert(adc.version==1&&adc.bytes==24&&adc.phase==1&&adc.clock_first==clock_value&&adc.clock_second==clock_value&&!adc.secure_release);del=42;assert(ThermalHandoffEntry(0,0)==42&&create==1);return 0;}\n'''.replace('SOURCE',str(source)))
 subprocess.run(['cc','-std=c11','-fshort-wchar','-I',str(p),str(test),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('thermal handoff: stale record removal and unknown ownership: OK')
