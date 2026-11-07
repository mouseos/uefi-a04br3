#!/usr/bin/env python3
"""A protocol counterexample, not a claim of an observed hardware failure.
Run the actual bounded WACS2 reader with another owner scheduled between its
idle check and command. An idle poll is not cross-owner serialization.
"""
from pathlib import Path
import os, subprocess, tempfile
pkg=Path(__file__).resolve().parents[1]
header=pkg/'WindowsArm32/Include/PwrapRead.h'
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as directory:
    tmp=Path(directory);(tmp/'Library').mkdir()
    (tmp/'Library/IoLib.h').write_text('');(tmp/'Library/TimerLib.h').write_text('')
    (tmp/'Uefi.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef uint16_t UINT16;typedef uint32_t UINT32;typedef size_t UINTN;typedef int EFI_STATUS;
#define STATIC static
#define EFI_SUCCESS 0
#define EFI_TIMEOUT 1
#define EFI_ERROR(s) ((s)!=0)
static UINT32 MmioRead32(UINTN);static void MmioWrite32(UINTN,UINT32);static void MicroSecondDelay(UINTN);
''')
    code=r'''
#include "HEADER"
static unsigned fsm,response,collision,commands,clears,other_consumed;
static UINT32 MmioRead32(UINTN address){assert(address==WACS_DATA);UINT32 current=(fsm<<16)|response;
 /* Other CPU begins and finishes its read after this CPU sampled IDLE,
  * before this CPU writes CMD. RDATA carries no response address/tag. */
 if(collision && !fsm){collision=0;fsm=6;response=17;other_consumed=1;}
 return current;
}
static void MmioWrite32(UINTN address,UINT32 value){
 if(address==WACS_CLEAR){assert(value==1 && fsm==6);fsm=0;clears++;return;}
 assert(address==WACS_CMD && !(value&(1u<<31)) && ((value>>16)*2)==0x800a);commands++;
 /* A valid single-outstanding-channel model: CMD while busy is not accepted.
  * The existing reader cannot distinguish this from its own response. */
 if(!fsm){fsm=6;response=23;}
}
static void MicroSecondDelay(UINTN n){assert(n==1);}
int main(void){UINT16 value=0;
 assert(!ReadPmic(0x800a,&value) && value==23 && commands==1 && clears==1);
 collision=1;value=0;
 assert(!ReadPmic(0x800a,&value) && value==17 && other_consumed && commands==2 && clears==2);
 return 0;}
'''.replace('HEADER',str(header))
    (tmp/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','clang'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(tmp),str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('PASS protocol counterexample: production idle-check reader can consume an interleaved owner response. No device operation; this is not a silicon behavior measurement.')
