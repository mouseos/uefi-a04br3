#!/usr/bin/env python3
"""Check every selectable ACPI storage INF against the platform PCD default."""
from pathlib import Path
import re
import uuid

root = Path(__file__).resolve().parents[4]
platform = root / 'Platforms/Benesse/a04br3Pkg/WindowsArm32'
dec = (root / 'Mu_Basecore/MdeModulePkg/MdeModulePkg.dec').read_text()
pcd = re.search(r'PcdAcpiTableStorageFile\|\{([^}]+)\}', dec)
assert pcd, 'ACPI storage PCD missing'
expected = uuid.UUID(bytes_le=bytes(int(x.strip(), 16) for x in pcd.group(1).split(',')))
for filename in ('WindowsArm32.dsc', 'WindowsArm32.fdf'):
    assert 'PcdAcpiTableStorageFile' not in (platform / filename).read_text(), 'review platform PCD override'
fdf = (platform / 'WindowsArm32.fdf').read_text()
infs = set(re.findall(r'Acpi/(AcpiTables\w*\.inf)', fdf))
assert infs, 'no ACPI storage files selected by FDF'
for name in sorted(infs):
    text = (platform / 'Acpi' / name).read_text()
    match = re.search(r'^\s*FILE_GUID\s*=\s*([\w-]+)', text, re.M)
    assert match and uuid.UUID(match.group(1)) == expected, f'{name}: storage GUID mismatch'
    for required in ('Fadt.aslc', 'Gtdt.aslc', 'Madt.aslc', 'Dsdt.asl'):
        assert re.search(r'^\s*' + re.escape(required) + r'\s*$', text, re.M), f'{name}: missing {required}'
print(f'{len(infs)} ACPI storage variants match PCD {expected} and include required tables')
