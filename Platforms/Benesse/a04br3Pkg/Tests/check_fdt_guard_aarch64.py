#!/usr/bin/env python3
"""Run the real FDT guard with EL1 alignment checks on a QEMU virt CPU."""
import os,struct,subprocess,tempfile
from pathlib import Path

p=Path(__file__).resolve().parents[1]/'WindowsArm32/BootShim/FdtGuard.c'
prefix=os.environ.get('A04_AARCH64_PREFIX','aarch64-linux-gnu-')
with tempfile.TemporaryDirectory() as d:
 t=Path(d); src=t/'fixture.dts';dtb=t/'fixture.dtb'
 src.write_text('''/dts-v1/; / { #address-cells=<2>; #size-cells=<2>;
 memory@00000000 { device_type="memory"; reg=<0 0x40000000 0 0x20000000>; };
 chosen { linux,initrd-start=<0x44000000>; linux,initrd-end=<0x4420612d>; };
 reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges;
   test@43000000 { pad; reg=<0 0x43000000 0 0x30000>; }; }; };''')
 subprocess.run(['dtc','-q','-I','dts','-O','dtb','-o',str(dtb),str(src)],check=True)
 data=dtb.read_bytes(); so,strings=struct.unpack_from('>II',data,8);pos=so;path=[];misaligned=False
 while True:
  token=struct.unpack_from('>I',data,pos)[0];pos+=4
  if token==1:
   end=data.index(0,pos);path.append(data[pos:end].decode());pos=(end+4)&~3
  elif token==2:path.pop()
  elif token==3:
   size,offset=struct.unpack_from('>II',data,pos);pos+=8
   end=data.index(0,strings+offset);name=data[strings+offset:end].decode()
   if path==['','reserved-memory','test@43000000'] and name=='reg':misaligned=pos%8==4
   pos+=(size+3)&~3
  elif token==9:break
  elif token!=4:raise SystemExit('unexpected fixture token')
 if not misaligned: raise SystemExit('fixture did not place reg at four-byte alignment')
 asm=t/'start.S';asm.write_text('''
.text
.global _start
_start:
  adr x1, stack_top
  mov sp, x1
  adr x2, vectors
  msr vbar_el1, x2
  isb
  mrs x2, sctlr_el1
  orr x2, x2, #2
  msr sctlr_el1, x2
  isb
  adr x0, fixture
  mov x1, #FIXTURE_SIZE
  bl A04FdtGuard
  cbnz w0, failed
  adr x1, exit_ok
  b quit
failed:
  adr x1, exit_bad
quit:
  mov x0, #0x20
  hlt #0xf000
1: b 1b
.balign 8
exit_ok: .quad 0x20026, 0
exit_bad: .quad 0x20026, 1
.balign 8
fixture: .incbin "FIXTURE_PATH"
.balign 16
stack: .space 4096
stack_top:
.balign 2048
vectors:
.rept 16
  b exception
  .space 124
.endr
exception:
  adr x1, exit_bad
  mov x0, #0x20
  hlt #0xf000
2: b 2b
'''.replace('FIXTURE_SIZE',str(len(data))).replace('FIXTURE_PATH',str(dtb)))
 flags=['-O2','-ffreestanding','-fno-stack-protector','-fno-builtin','-fno-pic','-fno-pie','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only','-mstrict-align']
 subprocess.run([prefix+'gcc','-c',str(asm),'-o',str(t/'start.o')],check=True)
 old=t/'previous.c'
 old.write_bytes(subprocess.check_output(['git','show','1c9d7ebe:Platforms/Benesse/a04br3Pkg/WindowsArm32/BootShim/FdtGuard.c']))
 for label,source,options,expected in [('previous',old,flags[:-1],1),('corrected',p,flags,0)]:
  subprocess.run([prefix+'gcc',*options,'-c',str(source),'-o',str(t/'guard.o')],check=True)
  subprocess.run([prefix+'ld','-Ttext','0x40080000','-e','_start','-o',str(t/'image.elf'),str(t/'start.o'),str(t/'guard.o')],check=True)
  subprocess.run([prefix+'objcopy','-O','binary',str(t/'image.elf'),str(t/'image.bin')],check=True)
  r=subprocess.run(['qemu-system-aarch64','-M','virt','-cpu','cortex-a53','-m','128M','-nographic','-monitor','none','-serial','null','-semihosting','-kernel',str(t/'image.bin')],timeout=8)
  if r.returncode!=expected: raise SystemExit(f'{label} QEMU exit {r.returncode}, expected {expected}')
print('QEMU SCTLR_EL1.A: previous guard faults; corrected guard accepts four-byte-aligned reg')
