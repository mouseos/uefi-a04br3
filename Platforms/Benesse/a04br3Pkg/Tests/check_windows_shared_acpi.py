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
assert 'DEFINE A04BR3_SHARED_APMIXED = 0' in dsc
assert '$(A04BR3_THERMAL_PROVIDER) != 0' in dsc

# Exercise the actual DSC guard against every combination, including thermal.
guard=next(line[4:] for line in dsc.splitlines() if line.startswith('!if $(A04BR3_SHARED_APMIXED) == 1 &&'))
def rejected(values):
    expression=re.sub(r'\$\((A04BR3_[A-Z_]+)\)',lambda m:str(values[m.group(1)]),guard)
    expression=expression.replace('&&',' and ').replace('||',' or ')
    assert re.fullmatch(r'[01 ()=!andor]+',expression),expression
    return bool(eval(expression,{'__builtins__':{}},{}))
import itertools
guard_names=('A04BR3_SHARED_APMIXED','A04BR3_SHARED_PARENT','A04BR3_THERMAL_PROVIDER','A04BR3_GPU_HANDOFF','A04BR3_BATTERY_PROVIDER','A04BR3_DISPLAY_PROVIDER')
for bits in itertools.product((0,1),repeat=len(guard_names)):
    v=dict(zip(guard_names,bits))
    want=v['A04BR3_SHARED_APMIXED'] and (not v['A04BR3_SHARED_PARENT'] or any(v[x] for x in guard_names[2:]))
    assert rejected(v)==bool(want),v

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
names=('A04BR3_SHARED_PARENT','A04BR3_BATTERY_PROVIDER','A04BR3_GPU_HANDOFF','A04BR3_DISPLAY_PROVIDER','A04BR3_TOUCH_PROVIDER','A04BR3_SHARED_APMIXED')
for bits in itertools.product((0,1),repeat=len(names)):
    values=dict(zip(names,bits))
    if values['A04BR3_SHARED_PARENT'] and values['A04BR3_TOUCH_PROVIDER']: continue
    if values['A04BR3_SHARED_APMIXED'] and (not values['A04BR3_SHARED_PARENT'] or values['A04BR3_GPU_HANDOFF'] or values['A04BR3_BATTERY_PROVIDER'] or values['A04BR3_DISPLAY_PROVIDER']): continue
    if values['A04BR3_DISPLAY_PROVIDER'] and not values['A04BR3_GPU_HANDOFF']: continue
    chosen=selection(dsc,values)
    assert chosen==selection(fdf,values)
    table=(pkg/'Acpi'/chosen).read_text()
    assert ('SharedParent.asl' in table)==bool(values['A04BR3_SHARED_PARENT'] and not values['A04BR3_SHARED_APMIXED'])
    assert ('SharedParentApmixed.asl' in table)==bool(values['A04BR3_SHARED_APMIXED'])
    assert ('ButtonArray.asl' in table)==bool(values['A04BR3_SHARED_PARENT'])
    assert ('TouchProvider.asl' in table)==bool(values['A04BR3_TOUCH_PROVIDER'])
    assert 'ButtonProvider.asl' not in table
    for name,source in [('A04BR3_BATTERY_PROVIDER','BatteryGauge.asl'),('A04BR3_GPU_HANDOFF','GpuProvider.asl'),('A04BR3_DISPLAY_PROVIDER','DisplayProvider.asl')]:
        assert (source in table)==bool(values[name]),(values,chosen,source)
with tempfile.TemporaryDirectory() as tmp:
    for variant in ('SharedParent','SharedParentApmixed'):
        files=[]
        for name in ('Dsdt',variant,'ButtonArray'):
            out=Path(tmp)/name
            subprocess.run(['iasl','-p',str(out),str(pkg/'Acpi'/f'{name}.asl')],check=True,capture_output=True)
            data=out.with_suffix('.aml').read_bytes();assert sum(data)%256==0
            assert b'MOU0003' not in data and b'MOU0004' not in data
            files.append(str(out.with_suffix('.aml')))
        r=subprocess.run(['acpiexec','-b','evaluate _SB.SHR0._HID; evaluate _SB.SHR0._CRS; evaluate _SB.BTT0._HID',*files],check=True,capture_output=True,text=True)
        assert '[String] Length 07 = "BEN0001"' in r.stdout and '[String] Length 07 = "MOU0002"' in r.stdout and 'AE_NOT_FOUND' not in r.stdout
        rows=re.findall(r'^\s*[0-9A-F]{4}:\s*((?:[0-9A-F]{2} )+)',r.stdout,re.M)
        blob=bytes.fromhex(' '.join(rows));expected=[(0x10000000,0x1000),(0x10001000,0x1000),(0x10005000,0x1000),(0x1000f000,0x1000)]
        if variant=='SharedParentApmixed':expected.append((0x10018000,0x710))
        assert len(blob)==len(expected)*12+2 and blob[-2:]==b'\x79\x00'
        for i,(address,length) in enumerate(expected):
            t,l,access,start,size=struct.unpack_from('<BH BII',blob,i*12)
            assert (t,l,access,start,size)==(0x86,9,1,address,length)
print('Canonical ARM32 shared parent AML and resource namespace validated; hardware untested')
