#!/usr/bin/env python3
"""Compile the board brightness resource both ways; no device operations."""
import argparse,json,re,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True)
pkg=Path(__file__).resolve().parents[1];source=pkg/'WindowsArm32/Acpi/DisplayProvider.asl'
# No other board ACPI owner may overlap DISP_PWM0, including legacy fragments.
for other in source.parent.glob('*.asl'):
 if other==source:continue
 for base,size in re.findall(r'Memory32Fixed\s*\(ReadWrite,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)',other.read_text()):
  base,size=int(base,16),int(size,16)
  assert base+size<=0x1100f000 or base>=0x11010000,other
results=[]
for brightness in (0,1):
 for gce in (0,1):
  flags=[]
  if brightness:flags+=['-DA04BR3_DISPLAY_BRIGHTNESS_RESOURCES=1']
  if gce:flags+=['-DA04BR3_DISPLAY_GCE_RESOURCES=1']
  s=subprocess.check_output(['cpp','-P',*flags,str(source)],text=True)
  target=a.out/f'display-brightness-{brightness}-gce-{gce}'
  target.with_suffix('.asl').write_text(s)
  r=subprocess.run(['iasl','-p',str(target),str(target.with_suffix('.asl'))],capture_output=True,text=True)
  target.with_suffix('.log').write_text(r.stdout+r.stderr);assert r.returncode==0
  aml=target.with_suffix('.aml').read_bytes();assert sum(aml)%256==0
  ranges=[(int(x,16),int(n,16)) for x,n in re.findall(r'Memory32Fixed\s*\(ReadWrite,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)',s)]
  assert ranges[:4]==[(0x14007000,4096),(0x14009000,4096),(0x14012000,4096),(0x14015000,4096)]
  assert ranges.count((0x1100f000,4096))==brightness
  assert aml.count(struct.pack('<BHBII',0x86,9,1,0x1100f000,4096))==brightness
  for i,(base,size) in enumerate(ranges):
   for other,amount in ranges[:i]:assert base+size<=other or other+amount<=base
  assert '"BEN0001"' not in s and '"MOU0006"' not in s
  results.append({'brightness':brightness,'gce':gce,'ranges':ranges,'aml_checksum':'passed'})
(a.out/'validation.json').write_text(json.dumps(results,indent=2)+'\n')
print('Brightness resource OFF/ON and GCE OFF/ON: iasl/checksum/ranges passed')
