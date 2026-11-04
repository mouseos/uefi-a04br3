#!/usr/bin/env python3
"""Check candidate board ranges and actual map entries, not runtime allocation."""
from pathlib import Path
import re
p = Path(__file__).resolve().parents[1]/'WindowsArm32'
dsc = (p/'Arm32Memory.dsc.inc').read_text()
fields = dict(re.findall(r'\.(Pcd\w+)\|(0x[0-9A-Fa-f]+)', dsc))
base, size, stack, ss = [int(fields[k], 16) for k in
    ('PcdA04WinHobBase','PcdA04WinHobSize','PcdPrePiStackBase','PcdPrePiStackSize')]
assert base + size == stack + ss == 0x52000000 and stack == 0x51fc0000
text = (p/'Library/MemoryMapLib/MemoryMapLib.c').read_text()
entries = {name:(int(addr,16),int(length,16)) for name,addr,length in re.findall(
    r'\{"([^"]+)",\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)', text)}
assert entries['ARM32 HOB Heap'] == (base, stack-base)
assert entries['ARM32 Stack'] == (stack, ss)
assert entries['I2C1 Accelerometer'] == (0x1100a000, 0x1000)
for name in ('UEFI FD','LK DTB','ATF Reserved','Boot Ramdisk','Display Reserved'):
    start,length = entries[name]
    assert base+size <= start or start+length <= base, name
assert base % 0x4000 == 0 and stack % 0x1000 == 0
print('Candidate HOB/stack/map ranges do not overlap fixed reservations; runtime unverified')
