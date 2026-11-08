#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Production producer control-flow model, not real MMIO/reset/temperature."""
from pathlib import Path
import subprocess,argparse,os,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
r=Path(subprocess.check_output(['git','rev-parse','--show-toplevel'],cwd=Path(__file__).parent,text=True).strip());o=Path(a.output).resolve();o.mkdir(parents=True,exist_ok=True)
src=r/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Drivers/ThermalHandoffDxe';s=(src/'ThermalPioHandoff.c').read_text();(o/'body.inc').write_text('\n'.join(x for x in s.splitlines() if not x.startswith('#include')))
(o/'test.cpp').write_text(r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
#define STATIC static
#define VOID void
#define TRUE true
#define FALSE false
#define MAX_UINT32 UINT32_MAX
#define DEBUG_ERROR 1
#define DEBUG(x) ((void)0)
using UINT32=uint32_t;using UINT16=uint16_t;using UINTN=uintptr_t;using BOOLEAN=bool;using EFI_STATUS=unsigned;
struct EFI_GUID{uint32_t a;uint16_t b,c;uint8_t d[8];};
#include "ThermalPioRecord.h"
struct E{unsigned kind;UINTN address;UINT32 value;};
static std::vector<E> trace;static std::map<UINTN,UINT32> regs;static unsigned fail;static A04_THERMAL_PIO_RECORD published;
UINT32 MmioRead32(UINTN a){assert(regs.count(a));auto v=regs[a];trace.push_back({0,a,v});return v;}
UINT16 MmioRead16(UINTN a){assert(regs.count(a));auto v=regs[a];trace.push_back({2,a,v});return (UINT16)v;}
void ArmDataSynchronizationBarrier(){trace.push_back({4,0,0});}
void MmioWrite32(UINTN a,UINT32 v){trace.push_back({1,a,v});
 if(a==0x10000084U){if(fail!=1)regs[0x10000024U]&=~v;}
 else if(a==0x10001120U){if(fail!=2)regs[0x10001128U]|=v;}
 else if(a==0x10001124U){if(fail!=3)regs[0x10001128U]&=~v;}
 else{assert(regs.count(a));regs[a]=v;}}
void MmioWrite16(UINTN a,UINT16 v){assert(regs.count(a));trace.push_back({3,a,v});regs[a]=v;}
void stall(UINTN){}
EFI_STATUS save(const wchar_t*,EFI_GUID*,unsigned attr,size_t bytes,void*p){assert(attr==6&&bytes==72);memcpy(&published,p,bytes);return 0;}
struct BS{void(*Stall)(UINTN);};struct RT{EFI_STATUS(*SetVariable)(const wchar_t*,EFI_GUID*,unsigned,size_t,void*);};BS bs={stall};RT rt={save};BS*gBS=&bs;RT*gRT=&rt;
#include "body.inc"
static void setup(){regs={{TOP+0x24,0x40000002},{I+0x128,0},{T,0},{T+0x3c,0x10e},{T+0x418,0},{T+0xc,0},{T+0x10,0},{A,0},{A+4,0},{A+0x10,0}};trace.clear();mAttempted=false;fail=0;memset(&published,0,sizeof(published));}
static std::vector<E>writes(){std::vector<E>w;for(size_t i=0;i<trace.size();++i)if(trace[i].kind==1||trace[i].kind==3){assert(i+1<trace.size()&&trace[i+1].kind==4);w.push_back(trace[i]);}return w;}
int main(){
 // One negative boundary for each independent required stop condition.
 const UINTN bad[]={T+0x3c,T,T+0x418,T+0xc,T+0x10,A,A+4,A+0x10};
 for(auto a:bad){setup();regs[a]=a==T+0x3c?0:a==T+0x418?0x10000:1;A04ThermalPioBeforeExit();assert(writes().empty()&&published.stage==1&&!published.ready);}
 setup();A04ThermalPioBeforeExit();auto w=writes();assert(w.size()==9&&w[0].address==TOP+0x84&&w[1].address==I+0x120&&w[2].address==I+0x124);assert(published.ready==1&&published.stage==6&&(published.reset_after&1));
 auto count=trace.size();A04ThermalPioBeforeExit();assert(trace.size()==count); // once only
 for(unsigned f=1;f<=3;++f){setup();fail=f;A04ThermalPioBeforeExit();w=writes();assert(!published.ready&&published.stage==1);assert(w.size()==f);}
 setup();regs[I+0x128]=1;A04ThermalPioBeforeExit();w=writes();assert(w[0].address==I+0x120&&w[1].address==TOP+0x84&&w[2].address==I+0x124);assert(published.ready==1);
 puts("PASS production flow: failed stop guards write nothing; qualified probe-order/legacy-order; strict ungate/assert/deassert refusal; final reset/72B/once/barriers. MODEL ONLY, no hardware proof.");
}
''')
env=dict(os.environ,TMPDIR=str(o));subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-Wno-unused-but-set-variable','-fsanitize=address,undefined','-I'+str(src),str(o/'test.cpp'),'-o',str(o/'test')],check=True,env=env);subprocess.run([str(o/'test')],check=True,env=env)
(o/'result.json').write_text(json.dumps({'source_sha256':hashlib.sha256((src/'ThermalPioHandoff.c').read_bytes()).hexdigest(),'guard_failures_write_nothing':True,'qualified_order_and_strict_failures':True,'model_only':True,'hardware_tested':False,'unproved_gated_reads_not_claimed_as_hardware_success':True},indent=2)+'\n')
