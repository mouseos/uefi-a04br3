#!/usr/bin/env python3
"""Execute real AArch64 CPU1 handoff instructions with emulated MMIO."""
import os,struct,subprocess,tempfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm64_const import *

board=Path(__file__).resolve().parents[1]
prefix=os.environ.get('A04_AARCH64_PREFIX','aarch64-linux-gnu-')
scratch=Path(os.environ.get('A04_TEST_TMPDIR',str(board.parents[2]/'Build')))
scratch.mkdir(parents=True,exist_ok=True)
flags={'UEFI_BASE':'0x41080000','UEFI_SIZE':'0x300000','A04BR3_A64_CPU_TEST':'1',
 'A04BR3_A32_SECONDARY_TEST':'1','A04BR3_PARK_TEST':'1','A04BR3_SMP_TEST':'1',
 'A04BR3_WDT_HANDOFF_TEST':'0','A04BR3_FOUR_CPU_TEST':'1','A04BR3_A64_WDT_TEST':'1',
 'A04BR3_A64_WDT_SECONDS':'420','A04BR3_KERNEL_MARKER':'1','A04BR3_FB_MONITOR':'1',
 'A04BR3_OFFLINE_WDT_UNBOUNDED':'0','A04BR3_FOUR_CPU_L1_CLEAN':os.environ.get('A04_TEST_CPU1_L1','0'),'A04BR3_WORKER_NO_UART_RESET':'0',
 'A04BR3_WORKER_HEARTBEAT':'1','A04BR3_EARLY_FDT_GUARD':'0'}
with tempfile.TemporaryDirectory(prefix='fourcpu-owner-',dir=scratch) as d:
 t=Path(d); obj=t/'shim.o';binary=t/'shim.bin'
 subprocess.run([prefix+'gcc','-c',str(board/'WindowsArm32/BootShim/BootShim.S'),'-o',str(obj),*[f'-D{k}={v}' for k,v in flags.items()]],check=True)
 subprocess.run([prefix+'objcopy','-O','binary','-j','.text',str(obj),str(binary)],check=True)
 nm=subprocess.check_output([prefix+'nm','-n',str(obj)],text=True)
 symbols={parts[-1]:int(parts[0],16) for line in nm.splitlines()
          if len(parts:=line.split())==3 and parts[1].lower()=='t'}
 start=symbols['_A64HandoffTestStart'];currentel=symbols['_A64HandoffTestCurrentEL']
 ic=symbols['_A64HandoffTestInvalidate'];skip=symbols['_A64HandoffTestSkip']
 entry=symbols['_A64EnterA32Parking']; helper=symbols.get('_A64CleanInvalidateLocalL1')
 code=binary.read_bytes(); base=0x100000
 def run(enabled,mailbox=1,dlab=False,ready=True):
  u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
  for address,size in ((base,0x20000),(0x10007000,0x1000),(0x11005000,0x1000),(0x500000,0x1000),(0x600000,0x1000)):
   u.mem_map(address,size)
  u.mem_write(base,code);u.mem_write(0x10007000,struct.pack('<I',enabled));
  u.mem_write(0x1100500c,struct.pack('<I',0x80 if dlab else 0));u.mem_write(0x11005014,struct.pack('<I',0x20 if ready else 0))
  u.mem_write(0x500024,struct.pack('<I',0x600000));u.mem_write(0x600000,struct.pack('<IIII',mailbox,0,0x40100000,0))
  for reg,val in ((UC_ARM64_REG_X7,0x10007008),(UC_ARM64_REG_X8,0x1971),(UC_ARM64_REG_X15,0x11005000),
                  (UC_ARM64_REG_X19,0x500000),(UC_ARM64_REG_X4,26000000),(UC_ARM64_REG_X12,123456)):
   u.reg_write(reg,val)
  writes=[];reached=[];cleaned=[]
  def mem(uc,access,address,size,value,user):
   if address in (0x10007008,0x11005000):writes.append((address,value))
  def hook(uc,address,size,user):
   off=address-base
   if off==currentel:uc.reg_write(UC_ARM64_REG_X22,8);uc.reg_write(UC_ARM64_REG_PC,address+4)
   elif helper is not None and off==helper:
    cleaned.append(True);uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_X30))
   elif off==ic:uc.reg_write(UC_ARM64_REG_PC,address+4)
   elif off in (skip,entry):reached.append(off);uc.emu_stop()
  u.hook_add(UC_HOOK_CODE,hook);u.hook_add(UC_HOOK_MEM_WRITE,mem)
  u.emu_start(base+start,base+len(code),count=20000)
  assert reached, 'handoff did not terminate in the bounded instruction budget'
  assert u.reg_read(UC_ARM64_REG_X4)==26000000 and u.reg_read(UC_ARM64_REG_X12)==123456
  assert bool(cleaned)==(helper is not None and reached[0]==entry), 'L1 maintenance must run only on a valid handoff'
  return reached[0],writes,bytes(u.mem_read(0x500030,4))
 stop,writes,flag=run(0)
 assert stop==skip and flag==b'\1\0\0\0' and (0x10007008,0x1971) not in writes
 assert bytes(v for a,v in writes if a==0x11005000)==b'~6\n'
 stop,writes,_=run(1)
 assert stop==entry and (0x10007008,0x1971) in writes
 assert bytes(v for a,v in writes if a==0x11005000)==b'~5\n'
 stop,writes,_=run(1,mailbox=2)
 assert stop==skip and not writes
 for dlab,ready in ((True,True),(False,False)):
  stop,writes,_=run(1,dlab=dlab,ready=ready)
  assert stop==entry and (0x10007008,0x1971) in writes and not any(a==0x11005000 for a,v in writes)
print('AArch64 emulation: actual CPU1 handoff rejects disabled watchdog, honors mailbox and handles unavailable UART')
