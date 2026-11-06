#!/usr/bin/env python3
"""Exercise the opt-in CPU1 L1 maintenance instructions with emulated registers."""
import os, re, subprocess, tempfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X11, UC_ARM64_REG_X19, UC_ARM64_REG_X30, UC_ARM64_REG_PC
board = Path(__file__).resolve().parents[1]
prefix = os.environ.get('A04_AARCH64_PREFIX', 'aarch64-linux-gnu-')
scratch = Path(os.environ['A04_TEST_TMPDIR']); scratch.mkdir(parents=True, exist_ok=True)
flags = {'UEFI_BASE':'0x41080000', 'UEFI_SIZE':'0x300000', 'A04BR3_A64_CPU_TEST':'1',
 'A04BR3_A32_SECONDARY_TEST':'1', 'A04BR3_PARK_TEST':'1', 'A04BR3_SMP_TEST':'1',
 'A04BR3_WDT_HANDOFF_TEST':'0', 'A04BR3_FOUR_CPU_TEST':'1', 'A04BR3_FOUR_CPU_L1_CLEAN':'1',
 'A04BR3_A64_WDT_TEST':'1', 'A04BR3_A64_WDT_SECONDS':'420', 'A04BR3_KERNEL_MARKER':'1',
 'A04BR3_FB_MONITOR':'1', 'A04BR3_WORKER_HEARTBEAT':'1', 'A04BR3_EARLY_FDT_GUARD':'0'}
with tempfile.TemporaryDirectory(prefix='fourcpu-l1-', dir=scratch) as d:
 t=Path(d); obj=t/'shim.o'; raw=t/'shim.bin'
 subprocess.run([prefix+'gcc','-c',str(board/'WindowsArm32/BootShim/BootShim.S'),'-o',str(obj),*[f'-D{k}={v}' for k,v in flags.items()]],check=True)
 subprocess.run([prefix+'objcopy','-O','binary','-j','.text',str(obj),str(raw)],check=True)
 dis=subprocess.check_output([prefix+'objdump','-d',str(obj)],text=True)
 section=dis.split('<_A64CleanInvalidateLocalL1>:',1)[1].split('\n\n',1)[0]
 ins={int(m.group(1),16):m.group(2).strip() for line in section.splitlines() if (m:=re.match(r'\s*([0-9a-f]+):\s+[0-9a-f]+\s+(.*)',line))}
 start=min(ins); end=max(ins)+4; code=raw.read_bytes(); base=0x100000; sentinel=base+0x1f000
 def run(ctype,ccidx,ways,sets):
  u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);u.mem_map(base,0x20000);u.mem_write(base,code);u.reg_write(UC_ARM64_REG_X30,sentinel);u.reg_write(UC_ARM64_REG_X19,0x61726368)
  ccsidr=2|((ways-1)<<3)|((sets-1)<<(32 if ccidx else 13));touched=[];selections=[];done=[]
  def hook(uc,address,size,data):
   off=address-base
   if address==sentinel:done.append(True);uc.emu_stop();return
   op=ins.get(off,'')
   if op.startswith('mrs'):
    if 'clidr_el1' in op:value,reg=ctype,UC_ARM64_REG_X0
    elif 'ccsidr_el1' in op:value,reg=ccsidr,UC_ARM64_REG_X1
    elif 'id_aa64mmfr2_el1' in op:value,reg=ccidx<<20,UC_ARM64_REG_X0
    else:raise AssertionError(op)
    uc.reg_write(reg,value);uc.reg_write(UC_ARM64_REG_PC,address+4)
   elif op.startswith('msr'):
    assert 'csselr_el1, xzr' in op;selections.append(0);uc.reg_write(UC_ARM64_REG_PC,address+4)
   elif op.startswith('dc'):
    assert 'cisw, x11' in op;touched.append(uc.reg_read(UC_ARM64_REG_X11));uc.reg_write(UC_ARM64_REG_PC,address+4)
  u.hook_add(UC_HOOK_CODE,hook);u.emu_start(base+start,sentinel+4,count=10000)
  assert done and u.reg_read(UC_ARM64_REG_X19)==0x61726368
  if ctype<2:assert not touched and not selections;return
  assert selections==[0]
  shift=(32-(ways-1).bit_length()) if ways>1 else 32
  expected={(way<<shift)|(s<<6) for way in range(ways) for s in range(sets)}
  assert len(touched)==ways*sets and set(touched)==expected and all((v&0xe)==0 for v in touched)
 for c in (0,1):run(c,0,2,4)
 for idx in (0,1):
  for w,s in ((1,1),(2,4),(4,8)):run(3,idx,w,s)
print('CPU1 L1: legacy/CCIDX geometry, cache absence, local level and mailbox preservation: OK')
