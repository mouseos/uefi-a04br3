#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""既存6c96と観測追加後の実sourceを実行し、MMIO trace/公開recordの不変性を確認。"""
import argparse, pathlib, subprocess, os, json, hashlib
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
r=pathlib.Path(subprocess.check_output(['git','rev-parse','--show-toplevel'],cwd=pathlib.Path(__file__).parent,text=True).strip());o=pathlib.Path(a.output).resolve();o.mkdir(parents=True,exist_ok=True)
rel='Platforms/Benesse/a04br3Pkg/WindowsArm32/Drivers/ThermalHandoffDxe/ThermalPioHandoff.c'
baseline=subprocess.check_output(['git','show','6c96c6f60149f2b01d068909c8d0c308e5b4f3ee:'+rel],cwd=r,text=True)
for name,s in [('baseline',baseline),('observed',(r/rel).read_text())]:
 s='\n'.join(line for line in s.splitlines() if not line.startswith('#include'))
 s=s.replace('mAttempted',name+'_attempted').replace(' Idle(', ' '+name+'_Idle(').replace('!Idle(', '!'+name+'_Idle(').replace('A04ThermalPioBeforeExit',name+'_run')
 (o/(name+'.inc')).write_text(s)
(o/'test.cpp').write_text(r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <cstdarg>
#include <vector>
#include <cstdio>
#define STATIC static
#define VOID void
#define TRUE true
#define FALSE false
#define MAX_UINT32 UINT32_MAX
#define DEBUG_ERROR 1
#define DEBUG(args) dbg args
using UINT32=uint32_t;using UINT16=uint16_t;using UINTN=uintptr_t;using BOOLEAN=bool;using EFI_STATUS=unsigned;
struct EFI_GUID{uint32_t a;uint16_t b,c;uint8_t d[8];};
#include "ThermalPioRecord.h"
struct Event {unsigned kind;UINTN addr;UINT32 value;bool operator==(const Event& b)const{return kind==b.kind&&addr==b.addr&&value==b.value;}};
static std::vector<Event> trace;static unsigned scenario,reads,phase;static UINT32 actual;static A04_THERMAL_PIO_RECORD published;
UINT32 MmioRead32(UINTN a){
 ++reads;UINT32 v=0;
 if(reads==1)v=0x40000002;if(reads==2)v=0;
 if(reads==3)v=scenario==1?0:1;
 if(reads==4)v=scenario==2?0x40000002:0;
 if(reads==5)v=scenario==3?1:0;
 if(reads>=6)v=1; // Stage2 IRQ readback rejects, to bound this stage1 test.
 trace.push_back({0,a,v});return v;
}
void MmioWrite32(UINTN a,UINT32 v){trace.push_back({1,a,v});}
UINT16 MmioRead16(UINTN a){trace.push_back({2,a,0});return 0;}
void MmioWrite16(UINTN a,UINT16 v){trace.push_back({3,a,v});}
void stall(UINTN n){trace.push_back({4,n,0});}
EFI_STATUS save(const wchar_t*,EFI_GUID*,unsigned attr,size_t bytes,void* p){assert(attr==6&&bytes==72);std::memcpy(&published,p,bytes);return 0;}
struct BS{void (*Stall)(UINTN);};struct RT{EFI_STATUS (*SetVariable)(const wchar_t*,EFI_GUID*,unsigned,size_t,void*);};
BS bs={stall};RT rt={save};BS* gBS=&bs;RT* gRT=&rt;
void dbg(unsigned,const char* fmt,...){
 if(std::strstr(fmt,"phase=")){
  if(std::strstr(fmt,"phase=1"))phase=1;if(std::strstr(fmt,"phase=2"))phase=2;if(std::strstr(fmt,"phase=3"))phase=3;
  va_list ap;va_start(ap,fmt);actual=va_arg(ap,UINT32);va_end(ap);
 }
}
#include "baseline.inc"
#include "observed.inc"
int main(){
 for(scenario=0;scenario<4;++scenario){
  trace.clear();reads=phase=actual=0;baseline_attempted=false;baseline_run();auto oldTrace=trace;auto oldRecord=published;
  trace.clear();reads=phase=actual=0;observed_attempted=false;observed_run();
  assert(trace==oldTrace);assert(std::memcmp(&oldRecord,&published,sizeof(published))==0);
  assert(phase==scenario);assert(published.ready==0);assert(published.stage==(scenario?1u:2u));
  if(scenario)assert(actual==(scenario==1?0u:scenario==2?0x40000002u:1u));
 }
 std::puts("PASS: three stage1 failures and stage1 pass preserve exact MMIO trace/72B record; failure phase/actual only");
}
''')
inc=r/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Drivers/ThermalHandoffDxe'
env=dict(os.environ,TMPDIR=str(o))
cmd=['g++','-std=c++17','-fsanitize=address,undefined','-g','-I'+str(inc),str(o/'test.cpp'),'-o',str(o/'test')]
subprocess.run(cmd,check=True,env=env);subprocess.run([str(o/'test')],check=True,env=env)
(o/'result.json').write_text(json.dumps({'status':'pass','source_sha256':hashlib.sha256((r/rel).read_bytes()).hexdigest(),'baseline_source':'6c96c6f60149f2b01d068909c8d0c308e5b4f3ee','cases':4,'exact_mmio_trace_and_record_preserved':True,'hardware_test':False},indent=2)+'\n')
