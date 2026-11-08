#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Production UEFI map coverage vs BeforeEBS accesses and ACPI owner ranges.
Source validation only; no MMIO or hardware emulation.
"""
from pathlib import Path
import re
pkg=Path(__file__).resolve().parents[1]
source=(pkg/'WindowsArm32/Library/MemoryMapLib/MemoryMapLib.c').read_text()
pattern=r'\{"([^"\n]+)",\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*AddDev,\s*MMAP_IO,\s*UNCACHEABLE,\s*MmIO,\s*NS_DEVICE\}'
regions=[(name,int(base,16),int(size,16)) for name,base,size in re.findall(pattern,source)]
asl=(pkg/'WindowsArm32/Acpi/SharedParentGpuThermal.asl').read_text()
owner=[(int(base,16),int(size,16)) for base,size in re.findall(r'Memory32Fixed\s*\(ReadWrite,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)',asl)]
assert len(owner)==8
# Six existing platform ranges + two thermal/ADC ranges remain one ACPI owner.
for base,size in owner:
 assert sum(start<=base and base+size<=start+length for _,start,length in regions)==1,hex(base)
accesses={0x1100d000:[(o,4) for o in (0,0xc,0x10,0x3c,0x418)],0x11003000:[(o,2) for o in (0,4,0x10)]}
for base,items in accesses.items():
 assert sum(start==base and length==4096 for _,start,length in regions)==1
 for offset,width in items:
  def covered(table):return sum(start<=base+offset and base+offset+width<=start+length for _,start,length in table)==1
  assert covered(regions)
  assert not covered([x for x in regions if x[1]!=base]) # missing-page regression
 # Adjacent pages must not be accidentally opened by a widened mapping.
 assert not any(start<=base-4096<start+length or start<=base+4096<start+length for _,start,length in regions)
converter=(pkg.parents[2]/'Silicon/Silicium/SiliciumPkg/Library/MemoryInitPeiLib/MemoryInitPei.c').read_text()
for field in ('PhysicalBase','VirtualBase'):
 assert re.search(r'MemoryTable\[Index\]\.'+field+r'\s*=\s*MemoryDescriptor\[i\]\.Address\s*;',converter)
assert 'MemoryDescriptor[i].ArmAttributes' in converter and 'ArmConfigureMmu (MemoryTable,' in converter
assert 'MemoryMapLib|a04br3Pkg/WindowsArm32/Library/MemoryMapLib/MemoryMapLib.inf' in (pkg/'WindowsArm32/Arm32Memory.dsc.inc').read_text()
assert 'gEfiEventBeforeExitBootServicesGuid' in (pkg/'WindowsArm32/Drivers/ThermalHandoffDxe/ThermalHandoffDxe.c').read_text()
assert 'HobOption == AddMem || HobOption == AddDev' in converter
assert 'BuildResourceDescriptorHob (MemoryRegion.ResourceType, MemoryRegion.ResourceAttribute, MemoryRegion.Address, MemoryRegion.Length)' in converter
assert 'MemoryRegion.ResourceType == EFI_RESOURCE_SYSTEM_MEMORY || MemoryRegion.MemoryType == EfiRuntimeServicesData' in converter
header=(pkg.parents[2]/'Silicon/Silicium/SiliciumPkg/Include/Library/MemoryMapLib.h').read_text()
for name,value in [('MMAP_IO','EFI_RESOURCE_MEMORY_MAPPED_IO'),('UNCACHEABLE','EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE'),('MmIO','EfiMemoryMappedIO'),('NS_DEVICE','ARM_MEMORY_REGION_ATTRIBUTE_DEVICE')]:
 assert re.search(r'#define\s+'+name+r'\s+'+value+r'\b',header)
print('PASS ARM32 thermal/ADC exact Device pages, eight owner ranges, access/neighbor boundaries, MMU identity inputs and MMIO resource HOB; no runtime allocation and no MMIO; source only')
