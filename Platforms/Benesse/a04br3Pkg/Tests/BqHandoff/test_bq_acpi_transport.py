#!/usr/bin/env python3
"""Validate the shipped AML template and relocation without touching hardware."""
from pathlib import Path
import re
import subprocess
import tempfile

pkg = Path(__file__).resolve().parents[2]
dxe = pkg / 'WindowsArm32/Drivers/BqI2c2HandoffDxe'
header = (dxe / 'BqHandoffAml.h').read_text()
values = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', header))
offset = int(re.search(r'A04_BQ_ADDRESS_OFFSET (\d+)u', header).group(1))
assert values[:4] == b'SSDT' and len(values) == int.from_bytes(values[4:8], 'little')
assert sum(values) % 256 == 0 and values.count(bytes.fromhex('c4b3a2d1')) == 1
assert values[offset:offset + 4] == bytes.fromhex('c4b3a2d1')
with tempfile.TemporaryDirectory(prefix='a04-bq-aml-') as td:
    out = Path(td) / 'bq'
    subprocess.run(['iasl', '-p', str(out), str(dxe / 'Acpi/BqHandoff.asl')], check=True, stdout=subprocess.DEVNULL)
    assert out.with_suffix('.aml').read_bytes() == values, 'AML differs from ASL; regenerate the template'
    relocated = bytearray(values)
    relocated[offset:offset + 4] = (0x12345000).to_bytes(4, 'little')
    relocated[9] = 0
    relocated[9] = (-sum(relocated)) & 255
    assert sum(relocated) % 256 == 0
    out.with_suffix('.aml').write_bytes(relocated)
    subprocess.run(['iasl', '-d', '-p', str(out), str(out.with_suffix('.aml'))], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    decoded = out.with_suffix('.dsl').read_text()
    assert 'BQHR' in decoded and 'BAT0' in decoded and '12345000' in decoded
    battery = Path(td) / 'battery'
    subprocess.run(['iasl', '-p', str(battery), str(pkg / 'Tests/BqHandoff/BatteryGauge.asl')], check=True, stdout=subprocess.DEVNULL)
    result = subprocess.run(['acpiexec', '-b', 'execute _SB.BAT0.BQHR',
                             str(battery.with_suffix('.aml')), str(out.with_suffix('.aml'))],
                            check=True, capture_output=True, text=True)
    assert '[Buffer] Length 80' in result.stdout and 'AE_NOT_FOUND' not in result.stdout
    reverse = subprocess.run(['acpiexec', '-b', 'execute _SB.BAT0.BQHR',
                              str(out.with_suffix('.aml')), str(battery.with_suffix('.aml'))],
                             check=True, capture_output=True, text=True)
    assert 'AE_NOT_FOUND' in reverse.stdout + reverse.stderr, 'reverse order must not be accepted'
print('BQ ACPI AML, relocation, checksum and method: PASS (host only)')
