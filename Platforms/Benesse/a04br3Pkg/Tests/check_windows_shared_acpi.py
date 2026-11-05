#!/usr/bin/env python3
"""Host namespace and resource test; does not assert target PnP or power."""
from pathlib import Path
import re
import struct
import subprocess
import tempfile
pkg=Path(__file__).resolve().parents[1]/'WindowsArm32'
dsc=(pkg/'WindowsArm32.dsc').read_text();fdf=(pkg/'WindowsArm32.fdf').read_text()
assert 'DEFINE A04BR3_SHARED_PARENT = 0' in dsc
assert '$(A04BR3_TOUCH_PROVIDER) == 1 && $(A04BR3_SHARED_PARENT) == 1' in dsc
assert '$(A04BR3_BUTTON_PROVIDER) != 0' in dsc
for text in (dsc,fdf):
    assert '!if $(A04BR3_SHARED_PARENT) == 1' in text
    assert 'AcpiTablesShared.inf' in text and 'AcpiTables.inf' in text
base=(pkg/'Acpi/AcpiTables.inf').read_text();shared=(pkg/'Acpi/AcpiTablesShared.inf').read_text()
assert 'SharedParent.asl' not in base and 'SharedParent.asl' in shared
assert 'ButtonArray.asl' not in base and 'ButtonArray.asl' in shared
assert not any(x in shared for x in ('ButtonProvider.asl','TouchProvider.asl'))
with tempfile.TemporaryDirectory() as tmp:
    files=[]
    for name in ('Dsdt','SharedParent','ButtonArray'):
        out=Path(tmp)/name
        subprocess.run(['iasl','-p',str(out),str(pkg/'Acpi'/f'{name}.asl')],check=True,capture_output=True)
        data=out.with_suffix('.aml').read_bytes();assert sum(data)%256==0
        assert b'MOU0003' not in data and b'MOU0004' not in data
        files.append(str(out.with_suffix('.aml')))
    r=subprocess.run(['acpiexec','-b','evaluate _SB.SHR0._HID; evaluate _SB.SHR0._CRS; evaluate _SB.BTT0._HID',*files],check=True,capture_output=True,text=True)
    assert '[String] Length 07 = "BEN0001"' in r.stdout and '[String] Length 07 = "MOU0002"' in r.stdout and 'AE_NOT_FOUND' not in r.stdout
    rows=re.findall(r'^\s*[0-9A-F]{4}:\s*((?:[0-9A-F]{2} )+)',r.stdout,re.M)
    blob=bytes.fromhex(' '.join(rows));assert len(blob)==50 and blob[-2:]==b'\x79\x00'
    ranges=[]
    for i in range(0,48,12):
        t,l,access,start,size=struct.unpack_from('<BH BII',blob,i)
        assert (t,l,access,size)==(0x86,9,1,0x1000)
        ranges.append(start)
    assert ranges==[0x10000000,0x10001000,0x10005000,0x1000f000]
print('Canonical ARM32 shared parent AML and resource namespace validated; hardware untested')
