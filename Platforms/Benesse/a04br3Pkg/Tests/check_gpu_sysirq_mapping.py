#!/usr/bin/env python3
"""Check the MT8167 GPU handoff's MMIO-map and IRQ polarity contract."""
from pathlib import Path
import re

pkg = Path(__file__).resolve().parents[1]
source = (pkg/'WindowsArm32/Drivers/GpuPowerDxe/GpuPowerDxe.c').read_text()
regions = (pkg/'WindowsArm32/Library/MemoryMapLib/MemoryMapLib.c').read_text()
acpi = (pkg/'WindowsArm32/Acpi/GpuProvider.asl').read_text()
spans = [(int(base,16), int(size,16)) for base,size in re.findall(
    r'\{"[^"]+",\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),[^\n]+NS_DEVICE', regions)]
addresses = {int(x,16) for x in re.findall(r'Mmio(?:Read|Write)32\((0x[0-9a-fA-F]+)U?', source)}
banks = re.search(r'Banks\[\]\s*=\s*\{([^}]+)\}',source)
assert banks, 'power-bank address list missing'
addresses.update(int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',banks[1]))
for address in addresses:
    assert any(base <= address and address+4 <= base+size for base,size in spans), \
        f'GPU handoff MMIO is not mapped as NS_DEVICE: {address:#x}'
# Linux MT8167 DTS: sysirq base 0x10200620, GPU SPI185 LEVEL_LOW.
word = 0x10200620 + (185//32)*4
bit = 185%32
assert word in addresses and f'BIT{bit}' in source
assert re.search(r'MmioWrite32\(0x10200634U,\s*PolarityBefore\s*\|\s*BIT25\)',source)
assert re.search(r'Interrupt\s*\(ResourceConsumer,\s*Level,\s*ActiveHigh,\s*Exclusive\)\s*\{\s*217\s*\}',acpi)
print('GPU handoff MMIO mapping and normalized GIC polarity contract passed')
