#!/usr/bin/env python3
"""Execute actual DXE callbacks/Core with mocked boot services and MMIO; no device."""
from pathlib import Path
import subprocess
import tempfile
import shutil
import argparse
import struct
a=argparse.ArgumentParser();a.add_argument("--device-root",type=Path);args=a.parse_args()
ROOT = Path(__file__).resolve().parents[1]
MOCK = r'''
#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32; typedef uint64_t UINT64;
typedef unsigned long UINTN; typedef int BOOLEAN; typedef unsigned EFI_TPL; typedef unsigned EFI_STATUS;
typedef struct {UINT32 a;UINT16 b,c;UINT8 d[8];} EFI_GUID;
typedef void VOID; typedef void *EFI_EVENT; typedef void *EFI_HANDLE; typedef void EFI_SYSTEM_TABLE;
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_UNSUPPORTED 3
#define EFI_ERROR(x) ((x)!=0)
#define MAX_UINT64 UINT64_MAX
#define TPL_NOTIFY 16
#define TPL_CALLBACK 8
#define EVT_NOTIFY_SIGNAL 1
#define EVT_TIMER 2
#define TimerPeriodic 1
#define DEBUG_ERROR 1
void logmock(unsigned, const char *, ...);
#define DEBUG(x) logmock x
void MemoryFence(); UINT16 MmioRead16(UINTN); UINT32 MmioRead32(UINTN);
void MmioWrite16(UINTN,UINT16); void MmioWrite32(UINTN,UINT32);
UINT64 GetPerformanceCounter(); UINT64 GetPerformanceCounterProperties(UINT64*,UINT64*);
UINT64 GetTimeInNanoSecond(UINT64); UINTN MicroSecondDelay(UINTN);
typedef void (*Callback)(EFI_EVENT,VOID*);
struct Services {
 EFI_STATUS (*InstallMultipleProtocolInterfaces)(EFI_HANDLE*,...);
 EFI_STATUS (*LocateProtocol)(EFI_GUID*,void*,void**);
 EFI_TPL (*RaiseTPL)(EFI_TPL); void (*RestoreTPL)(EFI_TPL);
 EFI_STATUS (*CreateEventEx)(unsigned,EFI_TPL,Callback,void*,void*,EFI_EVENT*);
 EFI_STATUS (*CreateEvent)(unsigned,EFI_TPL,Callback,void*,EFI_EVENT*);
 EFI_STATUS (*SetTimer)(EFI_EVENT,unsigned,UINT64); EFI_STATUS (*CloseEvent)(EFI_EVENT);
};
extern Services *gBS; extern int gEfiEventExitBootServicesGuid;
'''
CODE = r'''
#include <map>
#include <vector>
#include <cassert>
#include <fstream>
#include <iterator>
#include "A04BR3ChargerDxe.cpp"
static std::map<UINTN,unsigned> regs;
static std::vector<std::pair<UINTN,unsigned>> writes;
static UINT64 counter=1000; static unsigned stage,failStage,firstFault;
static Callback timerCb, exitCb; static EFI_TPL tpl;
static bool eventAlive[3],timerRunning,protocolPublished;
static unsigned regptr; static bool started, busy, extBad; static unsigned kicks;
static UINT8 raw[8]={0x30,0,0x8c,0xa4,0x45,0x51,0,0x38};
extern "C" {
Services *gBS; int gEfiEventExitBootServicesGuid;
void logmock(unsigned,const char*,...) {}
void MemoryFence() {}
UINT16 MmioRead16(UINTN a) {
 if(a==0x1100b00c) return started?1:0;
 if(a==0x1100b030) return 16;
 if(a==0x1100b000) {UINT8 v=raw[regptr];if(!regptr){firstFault=v&7;raw[0]&=0xf8;}return v;}
 if(a==0x1100b024) return busy?1:0;
 if(a==0x1100b028&&extBad)return 0;
 return regs[a];
}
UINT32 MmioRead32(UINTN a) {return regs[a];}
void MmioWrite16(UINTN a,UINT16 v) {
 assert(stage>=3);writes.push_back({a,v});regs[a]=v;
 if(a==0x1100b038) {regptr=99;started=false;}
 if(a==0x1100b000){if(regptr==99)regptr=v;else{assert(regptr==0&&v==0x80);++kicks;}}
 if(a==0x1100b024)started=true;
}
void MmioWrite32(UINTN a,UINT32 v) {
 assert(stage>=3);writes.push_back({a,v});
 if(a==0x10000084)regs[0x10000024]&=~v;
 else if(a==0x100053c8)regs[0x100053c0]&=~v;
 else if(a==0x100053c4)regs[0x100053c0]|=v;
 else if(a==0x10005538)regs[0x10005530]&=~v;
 else if(a==0x10005914)regs[0x10005910]|=v;
 else {assert(a==0x10001080);regs[a]=v;}
}
UINT64 GetPerformanceCounter(){return counter;}
UINT64 GetPerformanceCounterProperties(UINT64*f,UINT64*l){*f=0;*l=MAX_UINT64;return 1000000000;}
UINT64 GetTimeInNanoSecond(UINT64 t){return t;}
UINTN MicroSecondDelay(UINTN t){counter+=t*1000;return t;}
}
static EFI_STATUS install(EFI_HANDLE*,...){
 assert(stage==3&&eventAlive[1]&&eventAlive[2]&&timerRunning);
 if(failStage==4)return 9;
 protocolPublished=true;return 0;
}
static EFI_STATUS locate(EFI_GUID*,void*,void **interface){
 if(!protocolPublished){*interface=nullptr;return 14;}
 *interface=&mHook;return 0;
}
static EFI_TPL raise(EFI_TPL v){auto prev=tpl;tpl=v;return prev;}
static void restore(EFI_TPL v){tpl=v;}
static EFI_STATUS createEx(unsigned,EFI_TPL t,Callback cb,void*,void*,EFI_EVENT*e){
 assert(t==TPL_NOTIFY);++stage;if(stage==failStage)return 9;exitCb=cb;*e=(void*)1;eventAlive[1]=true;return 0;}
static EFI_STATUS create(unsigned,EFI_TPL t,Callback cb,void*,EFI_EVENT*e){
 assert(t==TPL_CALLBACK);++stage;if(stage==failStage)return 9;timerCb=cb;*e=(void*)2;eventAlive[2]=true;return 0;}
static EFI_STATUS set(EFI_EVENT,unsigned,UINT64 period){assert(period==100000000);++stage;if(stage==failStage)return 9;timerRunning=true;return 0;}
static EFI_STATUS close(EFI_EVENT e){unsigned id=unsigned(reinterpret_cast<UINTN>(e));assert(id==1||id==2);assert(eventAlive[id]);eventAlive[id]=false;if(id==2)timerRunning=false;return 0;}
static void reset(){
 eventAlive[1]=eventAlive[2]=timerRunning=protocolPublished=false;mTimer=mExitEvent=nullptr;
 mHandoff=nullptr;mLastPhysical=0;mLastResult=a04charger::Stopped;
 mOwner={};mArmed=mExited=FALSE;stage=failStage=0;counter=1000;kicks=0;
 regs.clear();writes.clear();regptr=99;started=busy=extBad=false;raw[0]=0x30;
 regs[0x10000000]=0x10;regs[0x10000024]=0xffffffff;regs[0x10001080]=0xffffffff;
 regs[0x100053c0]=0xffff0000;regs[0x10005530]=0xffffffff;
 regs[0x10005000]=1<<9; // EN9 output-low, detect inputs
}
int main(int argc,char **argv){
 assert(argc==2);std::ifstream file(argv[1],std::ios::binary);
 std::vector<uint8_t> tree((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
 auto blank=tree;auto slot=a04charger::findSlot(tree.data(),unsigned(tree.size()));assert(slot);
 Services services={install,locate,raise,restore,createEx,create,set,close};gBS=&services;
 // Actual callback reproducer: FW FAULT read clears IC history, a fresh kernel
 // owner must still refuse its sealed stopped predecessor, with ZERO new pet.
 reset();raw[0]=0x31;A04BR3ChargerDxeEntry(0,0);
 assert(AttachTree(tree.data(),tree.size())==0);Exit(0,0);
 a04charger::Owner kernel;Ops ops;auto before=writes.size();
 assert(!a04charger::adopt(kernel,slot,128,counter,1000000000,ops.now()));
 assert(a04charger::step(kernel,ops)==a04charger::Stopped&&writes.size()==before&&kicks==0);
 // Normal EBS handoff verifies last physical tick/frequency and original gap.
 tree=blank;slot=a04charger::findSlot(tree.data(),unsigned(tree.size()));
 reset();A04BR3ChargerDxeEntry(0,0);assert(AttachTree(tree.data(),tree.size())==0);
 counter+=1000000000;Exit(0,0);before=writes.size();Exit(0,0);Tick(0,0);assert(writes.size()==before);
 kernel={};counter+=1000000000;
 assert(a04charger::adopt(kernel,slot,128,counter,1000000000,ops.now()));
 assert(a04charger::step(kernel,ops)==a04charger::Maintained&&kicks==2);
 kernel={};assert(!a04charger::adopt(kernel,slot,128,mLastPhysical+30000000000ULL,1000000000,ops.now()));
 kernel={};assert(!a04charger::adopt(kernel,slot,128,counter,26000000,ops.now()));
 slot[120]^=1;kernel={};assert(!a04charger::adopt(kernel,slot,128,counter,1000000000,ops.now()));slot[120]^=1;
 kernel={};assert(!a04charger::adopt(kernel,slot,128,a04charger::get64(slot+24)+20000000000ULL,1000000000,ops.now()));
 kernel={};assert(!a04charger::adopt(kernel,nullptr,0,counter,1000000000,ops.now()));
 slot[4]^=1;kernel={};assert(!a04charger::adopt(kernel,slot,128,counter,1000000000,ops.now()));slot[4]^=1;
 // Valid handoff can expire while the kernel is setting up resources.
 kernel={};assert(a04charger::adopt(kernel,slot,128,counter,1000000000,ops.now()));
 counter+=20000000000ULL;before=writes.size();assert(a04charger::step(kernel,ops)==a04charger::Deadline&&writes.size()==before);
 // Loader failure must detach before freeing the final DT buffer.
 tree=blank;reset();A04BR3ChargerDxeEntry(0,0);assert(AttachTree(tree.data(),tree.size())==0);
 assert(AttachTree(nullptr,0)==0);assert(!mHandoff);

 for(unsigned failure=1;failure<=4;++failure){
  reset();failStage=failure;assert(A04BR3ChargerDxeEntry(0,0)==9);
  assert(writes.empty()&&!mOwner.attempted&&!mArmed);
  assert(!eventAlive[1]&&!eventAlive[2]&&!timerRunning);
  EFI_GUID guid=DARWIN_BOOT_BOARD_HANDOFF_GUID;void *published=reinterpret_cast<void *>(1);
  assert(gBS->LocateProtocol(&guid,nullptr,&published)==14&&published==nullptr);
 }
 reset();assert(A04BR3ChargerDxeEntry(0,0)==0);
 {EFI_GUID guid=DARWIN_BOOT_BOARD_HANDOFF_GUID;void *published=nullptr;
  assert(gBS->LocateProtocol(&guid,nullptr,&published)==0&&published==&mHook);
  assert(eventAlive[1]&&eventAlive[2]&&timerRunning);}
 // A policy refusal keeps a successfully published image/event owner resident.
 reset();raw[0]=0x31;assert(A04BR3ChargerDxeEntry(0,0)==0);
 assert(protocolPublished&&eventAlive[1]&&eventAlive[2]&&mOwner.stopped);
 reset();assert(A04BR3ChargerDxeEntry(0,0)==0);assert(kicks==1&&mOwner.kicks==1);
 // Exact dedicated masks preserve I2C0/1, PMIC, unrelated pin bits; calculated 100kHz max.
 assert(regs[0x10000024]==(0xffffffffU&~(1U<<16)));
 assert(regs[0x10001080]==(0xffffffffU&~8U));
 assert(regs[0x100053c0]==0xffff0009&&regs[0x10005530]==0xffffcfff);
 assert(regs[0x1100b070]==1&&regs[0x1100b020]==32&&regs[0x1100b028]==0x8001); // 13MHz/(2*2*33)=98.48k
 counter+=10000000000ULL;timerCb(0,0);assert(kicks==2);
 auto n=writes.size();tpl=TPL_NOTIFY;exitCb(0,0);tpl=TPL_CALLBACK;timerCb(0,0);
 assert(mExited&&!mArmed&&writes.size()==n);exitCb(0,0);assert(writes.size()==n);
 // Late timer never pets stale policy; stopped owner cannot resume.
 reset();A04BR3ChargerDxeEntry(0,0);counter+=20000000000ULL;n=writes.size();Tick(0,0);
 assert(mOwner.stopped&&kicks==1&&writes.size()==n);Tick(0,0);assert(writes.size()==n);
 // Fault is consumed by modeled IC but preserved in owner, no pet or policy repair.
 reset();raw[0]=0x31;A04BR3ChargerDxeEntry(0,0);assert(mOwner.first.raw[0]==0x31&&firstFault==1&&kicks==0);
 reset();regs[0x10000000]=0;A04BR3ChargerDxeEntry(0,0);assert(writes.empty()&&!mOwner.attempted);
 reset();extBad=true;A04BR3ChargerDxeEntry(0,0);assert(mOwner.stopped&&!mOwner.attempted&&kicks==0);
 reset();busy=true;A04BR3ChargerDxeEntry(0,0);assert(mOwner.stopped&&!mOwner.attempted&&kicks==0);
 reset();regs[0x10000000]=0x20;A04BR3ChargerDxeEntry(0,0);assert(mOwner.stopped&&!mOwner.attempted&&writes.empty());
 reset();raw[3]=0;A04BR3ChargerDxeEntry(0,0);assert(mOwner.stopped&&mOwner.first.raw[3]==0&&kicks==0);raw[3]=0xa4;
 assert(a04charger::mainRate(70U<<14)==1820000000ULL);
 assert(a04charger::mainRate((42U<<14)|(1U<<24))==546000000ULL);
 for(unsigned sel=0;sel<16;++sel){
  reset();regs[0x10000000]=sel<<4;regs[0x10018120]=1;regs[0x10018130]=1;regs[0x10018124]=42U<<14;
  A04BR3ChargerDxeEntry(0,0);
  bool valid=sel==1||sel==2||sel==4||sel==12;
  assert(bool(kicks)==valid);if(!valid)assert(writes.empty());
 }
 reset();regs[0x10000000]=0x20;regs[0x10018120]=1;regs[0x10018130]=1;regs[0x10018124]=70U<<14;
 A04BR3ChargerDxeEntry(0,0);assert(kicks==1); // mainpll_d11/2 branch
}
'''
with tempfile.TemporaryDirectory() as tmp:
    t=Path(tmp)
    (t/'mock.h').write_text(MOCK)
    for name in ['Uefi.h','Library/BaseLib.h','Library/DebugLib.h','Library/IoLib.h','Library/TimerLib.h','Library/UefiBootServicesTableLib.h','Guid/EventGroup.h']:
        p=t/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "mock.h"\n')
    (t/'test.cpp').write_text(CODE)
    subprocess.run([shutil.which('g++'),'-std=c++11','-DA04CHARGER_HOST_TEST=1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-I',str(t),'-I',str(ROOT/'Include'),'-I',str(ROOT/'Drivers/A04BR3ChargerDxe'),str(t/'test.cpp'),'-o',str(t/'test')],check=True)
    tree=t/'tree.adt'
    if args.device_root:
        subprocess.run(['python3',str(args.device_root/'scripts/make-a04br3-adt.py'),'--dram-size','0x80000000','--output',str(tree)],check=True)
    else:
        def prop(k,v): return k.encode().ljust(32,b'\0')+struct.pack('<I',len(v))+v+b'\0'*((-len(v))%4)
        chosen=struct.pack('<II',2,0)+prop('name',b'chosen\0')+prop('a04br3-charger-owner',bytes(128))
        tree.write_bytes(struct.pack('<II',1,1)+prop('name',b'device-tree\0')+chosen)
    subprocess.run([str(t/'test'),str(tree)],check=True)
# OFF removes both FV inclusion and component; early order strictly follows timer.
s=(ROOT/'a04br3.dsc').read_text();assert 'DEFINE A04BR3_CHARGER_OWNER = 0' in s
for name in ['DXE','APRIORI']:
    s=(ROOT/f'Include/{name}.inc').read_text()
    assert s.index('ArmPkg/Drivers/TimerDxe/TimerDxe.inf')<s.index('A04BR3ChargerDxe.inf')
    assert '!if $(A04BR3_CHARGER_OWNER) == 1' in s
print('PASS protocol lifetime: failed CreateEventEx/CreateEvent/SetTimer/Install leaves no discoverable protocol/events; success/refusal resident; handoff FAULT stop/deadline/seal/frequency, actual generated ADT slot, EXT_CONF readback refusal; actual DXE: owner-before-read, event failures no access, dedicated initialization masks/timing, first FAULT, late stop, EBS no MMIO; OFF FV/component guard')
