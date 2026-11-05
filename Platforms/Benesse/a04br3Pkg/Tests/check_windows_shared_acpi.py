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

# Evaluate the ACPI selection subtree in both build descriptions. This models
# only the equality conditionals in that subtree; unknown syntax must fail.
def selection(text, values):
    lines=text.splitlines()
    start=next(i for i,line in enumerate(lines) if line.strip() == '!if $(A04BR3_SHARED_PARENT) == 1' and any('AcpiTablesShared' in x for x in lines[i:i+12]))
    stack=[]; selected=[]
    for line in lines[start:]:
        line=line.strip()
        if line.startswith('!if '):
            match=re.fullmatch(r'!if \$\((A04BR3_[A-Z_]+)\) == 1',line)
            assert match, line
            stack.append(bool(values[match.group(1)]))
        elif line == '!else':
            assert stack
            stack[-1]=not stack[-1]
        elif line == '!endif':
            assert stack
            stack.pop()
            if not stack: break
        elif all(stack):
            match=re.search(r'(AcpiTables[A-Za-z]*\.inf)',line)
            if match: selected.append(match.group(1))
    else:
        raise AssertionError('unclosed ACPI selection')
    assert len(selected)==1,selected
    return selected[0]

import itertools
names=('A04BR3_SHARED_PARENT','A04BR3_BATTERY_PROVIDER','A04BR3_GPU_HANDOFF','A04BR3_DISPLAY_PROVIDER','A04BR3_TOUCH_PROVIDER')
for bits in itertools.product((0,1),repeat=len(names)):
    values=dict(zip(names,bits))
    if values['A04BR3_SHARED_PARENT'] and values['A04BR3_TOUCH_PROVIDER']: continue
    if values['A04BR3_DISPLAY_PROVIDER'] and not values['A04BR3_GPU_HANDOFF']: continue
    chosen=selection(dsc,values)
    assert chosen==selection(fdf,values)
    table=(pkg/'Acpi'/chosen).read_text()
    assert ('SharedParent.asl' in table)==bool(values['A04BR3_SHARED_PARENT'])
    assert ('ButtonArray.asl' in table)==bool(values['A04BR3_SHARED_PARENT'])
    assert ('TouchProvider.asl' in table)==bool(values['A04BR3_TOUCH_PROVIDER'])
    assert 'ButtonProvider.asl' not in table
    for name,source in [('A04BR3_BATTERY_PROVIDER','BatteryGauge.asl'),('A04BR3_GPU_HANDOFF','GpuProvider.asl'),('A04BR3_DISPLAY_PROVIDER','DisplayProvider.asl')]:
        assert (source in table)==bool(values[name]),(values,chosen,source)
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
