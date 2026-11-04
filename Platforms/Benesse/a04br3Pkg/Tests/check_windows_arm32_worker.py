#!/usr/bin/env python3
"""Compile actual board shim options and inspect the worker's timer branch."""
from pathlib import Path
import os
import subprocess
import tempfile

p = Path(__file__).resolve().parents[1]
source = p/'WindowsArm32/BootShim/BootShim.S'
prefix = os.environ.get('A04_AARCH64_PREFIX', 'aarch64-none-elf-')
flags = {'UEFI_BASE':'0x41080000', 'UEFI_SIZE':'0x700000', 'A04BR3_A64_CPU_TEST':'1',
         'A04BR3_A32_SECONDARY_TEST':'1', 'A04BR3_PARK_TEST':'1', 'A04BR3_SMP_TEST':'1',
         'A04BR3_WDT_HANDOFF_TEST':'0', 'A04BR3_FOUR_CPU_TEST':'0', 'A04BR3_A64_WDT_TEST':'1',
         'A04BR3_A64_WDT_SECONDS':'420', 'A04BR3_KERNEL_MARKER':'1', 'A04BR3_FB_MONITOR':'1'}
with tempfile.TemporaryDirectory(prefix='a04-worker-') as tmp:
    def compile_case(options, ok=True):
        out = Path(tmp)/'shim.o'
        args = [prefix+'gcc', '-c', str(source), '-o', str(out)]
        args += ['-D%s=%s' % pair for pair in options.items()]
        result = subprocess.run(args, capture_output=True, text=True)
        assert (result.returncode == 0) == ok, result.stderr
        if ok:
            dis = subprocess.check_output([prefix+'objdump', '-d', str(out)], text=True)
            return dis.split('<_A64SecondaryProbe>:',1)[1][:1600]
    bounded = compile_case({**flags,'A04BR3_OFFLINE_WDT_UNBOUNDED':'0'})
    unbounded = compile_case({**flags,'A04BR3_OFFLINE_WDT_UNBOUNDED':'1'})
    assert 'cmp\tx10, x6' in bounded and 'cmp\tx10, x6' not in unbounded
    for key in ('A04BR3_FOUR_CPU_TEST', 'A04BR3_WDT_HANDOFF_TEST'):
        compile_case({**flags, key:'1','A04BR3_OFFLINE_WDT_UNBOUNDED':'1'}, False)
    compile_case({**flags, 'A04BR3_A64_WDT_TEST':'0','A04BR3_OFFLINE_WDT_UNBOUNDED':'1'}, False)
print('3 CPU worker timer option and unsafe combinations checked; target behavior unverified')
