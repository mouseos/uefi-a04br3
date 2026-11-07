#!/usr/bin/env python3
"""Fail closed when the board's counter-only clock meets advertised sleep."""
import argparse, hashlib, json, re, struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build-output',type=Path);a=p.parse_args()
acpi=Path(__file__).resolve().parents[1]/'WindowsArm32/Acpi'
for source in acpi.glob('*'):
    if source.suffix not in ('.asl','.aslc','.h'):continue
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',source.read_text(),flags=re.S)
    if re.search(r'\b(?:_S[1-4]_?|_LPI|_CST)\b',text):
        raise SystemExit(f'RTC counter backend has no suspend compensation: {source}')
fadt=(acpi/'Fadt.aslc').read_text()
fields=re.findall(r'\.([A-Za-z0-9_]+)\s*=',fadt)
if fields!=['Header','PreferredPmProfile','Flags'] or not re.search(r'\.Flags\s*=\s*EFI_ACPI_5_0_HW_REDUCED_ACPI\s*\n?\s*}',fadt):
    raise SystemExit('RTC S0-only FADT contract changed; requalify or disable runtime counter')
if a.build_output:
    dirs=list((a.build_output/'ARM/a04br3Pkg/WindowsArm32/Acpi').glob('*/OUTPUT'))
    if len(dirs)!=1:raise SystemExit('Expected one selected ACPI producer')
    data=(dirs[0]/'Fadt.acpi').read_bytes()
    if data[:4]!=b'FACP' or len(data)<268 or struct.unpack_from('<I',data,112)[0]!=(1<<20) or any(data[244:268]):
        raise SystemExit('Built FADT advertises unsupported power semantics')
    for aml in dirs[0].glob('*.aml'):
        if any(name in aml.read_bytes() for name in (b'_S1_',b'_S2_',b'_S3_',b'_S4_',b'_LPI',b'_CST')):
            raise SystemExit(f'Built AML advertises sleep: {aml}')
    (a.build_output/'FV/rtc-s0-validation.json').write_text(json.dumps({'scope':'advertised ACPI only; not proof of counter continuity in deep idle','fadt_sha256':hashlib.sha256(data).hexdigest(),'suspend_compensation':False},indent=2)+'\n')
print('PASS board RTC continuous-S0 ACPI contract (suspend remains unsupported)')
