#!/usr/bin/env python3
"""Build linked diagnostic shim and check early call and payload boundaries."""
from pathlib import Path
import os,re,subprocess,tempfile
p=Path(__file__).resolve().parents[1]/'WindowsArm32/BootShim'
prefix=os.environ.get('A04_AARCH64_PREFIX','aarch64-linux-gnu-')
options={'UEFI_BASE':'0x41080000','UEFI_SIZE':'0x300000','A04BR3_A64_CPU_TEST':'1',
 'A04BR3_A32_SECONDARY_TEST':'1','A04BR3_PARK_TEST':'1','A04BR3_SMP_TEST':'1',
 'A04BR3_WDT_HANDOFF_TEST':'0','A04BR3_FOUR_CPU_TEST':'0','A04BR3_A64_WDT_TEST':'1',
 'A04BR3_A64_WDT_SECONDS':'120','A04BR3_KERNEL_MARKER':'0','A04BR3_FB_MONITOR':'0',
 'A04BR3_OFFLINE_WDT_UNBOUNDED':'0','A04BR3_WORKER_NO_UART_RESET':'0',
 'A04BR3_WORKER_HEARTBEAT':'1','A04BR3_EARLY_FDT_GUARD':'1'}
with tempfile.TemporaryDirectory() as d:
 t=Path(d); objs=[]
 for name in ('BootShim.S','FdtGuard.c','EarlyRecovery.c'):
  obj=t/(name+'.o'); args=[prefix+'gcc','-c',str(p/name),'-o',str(obj)]
  if name.endswith('.S'):args += [f'-D{k}={v}' for k,v in options.items()]
  else:args += ['-O2','-ffreestanding','-fno-stack-protector','-fno-builtin','-fno-pic','-fno-pie','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only','-mstrict-align']
  subprocess.run(args,check=True);objs.append(str(obj))
 script=t/'shim.ld';script.write_text('SECTIONS { . = 0; .text : { *(.text*) *(.rodata*) . = ALIGN(16); *(.payload*) } /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) } }\n')
 elf=t/'shim.elf';subprocess.run([prefix+'ld','-T',str(script),'-o',str(elf),*objs],check=True)
 dis=subprocess.check_output([prefix+'objdump','-d',str(elf)],text=True)
 start=dis.split('<_Start>:',1)[1].split('<_CopyLoop>:',1)[0]
 assert re.search(r'bl\s+[0-9a-f]+ <A04FdtGuard>',start)
 assert start.count('<_A04EarlyMark>') == 4
 be64=dis.split('<be64>:',1)[1].split('\n\n',1)[0]
 assert 'ldrb' in be64 and not re.search(r'\bldr\s+x\d+,\s*\[x0\]',be64)
 failure=dis.split('<_A04FdtFailure>:',1)[1].split('\n\n',1)[0]
 assert re.search(r'bl\s+[0-9a-f]+ <A04PrepareFastbootRecovery>',failure)
 assert 'There are no relocations' in subprocess.check_output([prefix+'readelf','-r',str(elf)],text=True)
 symbols=subprocess.check_output([prefix+'nm','-n',str(elf)],text=True)
 payload=int(next(x.split()[0] for x in symbols.splitlines() if x.endswith(' _Payload')),16)
 binary=t/'shim.bin';subprocess.run([prefix+'objcopy','-O','binary',str(elf),str(binary)],check=True)
 assert payload==binary.stat().st_size
print('linked guard call precedes FD copy, failure reaches recovery and payload offset matches')
