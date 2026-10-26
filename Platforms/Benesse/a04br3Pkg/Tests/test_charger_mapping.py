#!/usr/bin/env python3
"""Check the actual platform table covers charger MMIO as Device memory."""
from pathlib import Path
import re

pkg = Path(__file__).resolve().parents[1]
uefi = pkg.parents[2]
converter = (uefi / "Silicon/Silicium/SiliciumPkg/Library/MemoryInitPeiLib/MemoryInitPei.c").read_text()
for field in ("PhysicalBase", "VirtualBase"):
    assert re.search(r"MemoryTable\[Index\]\." + field + r"\s*=\s*MemoryDescriptor\[i\]\.Address\s*;", converter)
source = (pkg / 'Library/MemoryMapLib/MemoryMapLib.c').read_text()
rows = re.findall(r'\{"([^"]+)",\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*AddDev,\s*MMAP_IO,\s*UNCACHEABLE,\s*MmIO,\s*NS_DEVICE\}', source)
regions = [(name, int(base, 16), int(size, 16)) for name, base, size in rows]
# Includes the faulting diagnostic read and all controller/pin/clock windows
# used by actual A04BR3ChargerDxe + shared BoardCore initialization.
accesses = (0x10000000, 0x10000024, 0x10000084, 0x10001080,
            0x10005000, 0x10005210, 0x100053C8, 0x10005538, 0x10005914,
            0x10018120, 0x10018124, 0x10018130, 0x1100B000, 0x1100B070)
def covered(address, table):
    return sum(base <= address and address + 4 <= base + size for _, base, size in table) == 1
for address in accesses:
    assert covered(address, regions), hex(address)
for base, probe in ((0x10018000, 0x10018130), (0x1100B000, 0x1100B070)):
    assert sum(start == base and size == 4096 for _, start, size in regions) == 1
    assert not covered(probe, [row for row in regions if row[1] != base])
print('PASS actual Device table: charger MMIO coverage, exact 4KiB pages; missing-page negative controls')
