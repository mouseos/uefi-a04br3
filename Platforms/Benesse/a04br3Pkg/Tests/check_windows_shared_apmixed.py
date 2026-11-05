#!/usr/bin/env python3
"""Verify the opt-in shared parent resources; not a hardware test."""
from pathlib import Path
import re, struct, subprocess, tempfile
pkg=Path(__file__).resolve().parents[1]/'WindowsArm32'
for name in ('WindowsArm32.dsc','WindowsArm32.fdf'):
    text=(pkg/name).read_text()
    assert text.count('!if $(A04BR3_SHARED_APMIXED) == 1') >= 6
    for suffix in ('','Battery','BatteryGpu','BatteryGpuDisplay','Gpu','GpuDisplay'):
        assert f'AcpiTablesSharedApmixed{suffix}.inf' in text
        assert f'AcpiTablesShared{suffix}.inf' in text
for suffix in ('','Battery','BatteryGpu','BatteryGpuDisplay','Gpu','GpuDisplay'):
    s=(pkg/'Acpi'/f'AcpiTablesSharedApmixed{suffix}.inf').read_text()
    assert 'SharedParentApmixed.asl' in s and '\n  SharedParent.asl' not in s
with tempfile.TemporaryDirectory() as tmp:
    files=[]
    for name in ('Dsdt','SharedParentApmixed','ButtonArray'):
        p=Path(tmp)/name
        subprocess.run(['iasl','-p',str(p),str(pkg/'Acpi'/f'{name}.asl')],check=True,capture_output=True)
        data=p.with_suffix('.aml').read_bytes();assert sum(data)%256==0
        files.append(str(p.with_suffix('.aml')))
    r=subprocess.run(['acpiexec','-b','evaluate _SB.SHR0._HID; evaluate _SB.SHR0._CRS',*files],check=True,capture_output=True,text=True)
    assert '[String] Length 07 = "BEN0001"' in r.stdout and 'AE_NOT_FOUND' not in r.stdout
    rows=re.findall(r'^\s*[0-9A-F]{4}:\s*((?:[0-9A-F]{2} )+)',r.stdout,re.M)
    blob=bytes.fromhex(' '.join(rows));assert len(blob)==62 and blob[-2:]==b'\x79\x00'
    ranges=[]
    for i in range(0,60,12):
        t,l,access,start,size=struct.unpack_from('<BH BII',blob,i)
        assert (t,l,access)==(0x86,9,1)
        ranges.append((start,size))
    assert ranges==[(0x10000000,0x1000),(0x10001000,0x1000),(0x10005000,0x1000),(0x1000f000,0x1000),(0x10018000,0x714)]
print('shared parent APMIXED ACPI fixture validated; hardware untested')
