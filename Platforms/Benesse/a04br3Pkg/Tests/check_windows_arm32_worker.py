#!/usr/bin/env python3
"""Compile actual board shim options and inspect the worker's timer branch."""
from pathlib import Path
import os
import re
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
            body = dis.split('<_A64SecondaryProbe>:',1)[1]
            return re.split(r'\n[0-9a-f]+ <[^>]+>:\n', body, maxsplit=1)[0]
    bounded = compile_case({**flags,'A04BR3_OFFLINE_WDT_UNBOUNDED':'0'})
    unbounded = compile_case({**flags,'A04BR3_OFFLINE_WDT_UNBOUNDED':'1'})
    assert 'cmp\tx10, x6' in bounded and 'cmp\tx10, x6' not in unbounded
    assert 'str\tw16, [x7, #12]' in bounded
    four = compile_case({**flags, 'A04BR3_FOUR_CPU_TEST':'1', 'A04BR3_OFFLINE_WDT_UNBOUNDED':'0'})
    # Machine code must examine TOPRGU_MODE before the final kick and branch
    # to the disabled path instead of transferring CPU1 with no watchdog.
    mode=four.index('ldur\tw16, [x7, #-8]')
    final_kick=four.index('str\tw8, [x7]',mode)
    assert 'tbnz\tw16, #0' in four[mode:final_kick]
    assert 'str\tw16, [x19, #48]' in four[mode:final_kick]
    assert '<_A64HandoffMark>' in four[mode:final_kick]
    diagnostic = compile_case({**flags, 'A04BR3_KERNEL_MARKER':'0', 'A04BR3_FB_MONITOR':'0',
                               'A04BR3_A64_WDT_SECONDS':'120', 'A04BR3_OFFLINE_WDT_UNBOUNDED':'0',
                               'A04BR3_WORKER_NO_UART_RESET':'0', 'A04BR3_WORKER_HEARTBEAT':'1'})
    assert 'cmp\tx10, x6' in diagnostic and 'str\tw16, [x7, #12]' in diagnostic
    assert 'str\tw16, [x19, #44]' in diagnostic
    quiet = compile_case({**flags, 'A04BR3_KERNEL_MARKER':'0', 'A04BR3_FB_MONITOR':'0',
                          'A04BR3_OFFLINE_WDT_UNBOUNDED':'1', 'A04BR3_WORKER_NO_UART_RESET':'1'})
    assert 'str\tw16, [x19, #44]' in quiet
    assert 'str\tw16, [x7, #12]' not in quiet
    compile_case({**flags, 'A04BR3_OFFLINE_WDT_UNBOUNDED':'1', 'A04BR3_WORKER_NO_UART_RESET':'1'}, False)
    compile_case({**flags, 'A04BR3_KERNEL_MARKER':'0', 'A04BR3_FB_MONITOR':'0',
                  'A04BR3_OFFLINE_WDT_UNBOUNDED':'0', 'A04BR3_WORKER_NO_UART_RESET':'1'}, False)
    for key in ('A04BR3_FOUR_CPU_TEST', 'A04BR3_WDT_HANDOFF_TEST'):
        compile_case({**flags, key:'1','A04BR3_OFFLINE_WDT_UNBOUNDED':'1'}, False)
    compile_case({**flags, 'A04BR3_A64_WDT_TEST':'0','A04BR3_OFFLINE_WDT_UNBOUNDED':'1'}, False)
print('3 CPU worker timer option and unsafe combinations checked; target behavior unverified')
