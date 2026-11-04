#!/usr/bin/env python3
"""Check namespace identities only; no parking/MADT or hardware validation."""
from pathlib import Path
import re
import subprocess
import tempfile

pkg = Path(__file__).resolve().parents[1]
acpi = pkg/'WindowsArm32/Acpi'
with tempfile.TemporaryDirectory(prefix='a04-win-acpi-') as tmp:
    outputs = []
    for source in ('Dsdt.asl', 'CpuParkingFourSsdt.asl'):
        out = Path(tmp)/Path(source).stem
        subprocess.run(['iasl', '-p', str(out), str(acpi/source)], check=True, capture_output=True)
        outputs.append(str(out.with_suffix('.aml')))
    commands = '; '.join('evaluate _SB.CPU%d._UID' % n for n in range(4))
    result = subprocess.run(['acpiexec', '-b', commands, *outputs], check=True, capture_output=True, text=True)
    ids = [int(v, 16) for v in re.findall(r'\[Integer\] = ([0-9A-Fa-f]+)', result.stdout)]
    assert ids == [0, 1, 2, 3], ids
    three = Path(tmp)/'cpu3'
    subprocess.run(['iasl', '-p', str(three), str(acpi/'CpuParkingSsdt.asl')], check=True, capture_output=True)
    result = subprocess.run(['acpiexec', '-b', 'evaluate _SB.CPU0._UID; evaluate _SB.CPU2._UID; evaluate _SB.CPU3._UID', outputs[0], str(three.with_suffix('.aml'))], check=True, capture_output=True, text=True)
    ids = [int(v, 16) for v in re.findall(r'\[Integer\] = ([0-9A-Fa-f]+)', result.stdout)]
    assert ids == [0, 2, 3], ids
print('ACPI three/four CPU namespace UIDs verified; MADT and startup unverified')
