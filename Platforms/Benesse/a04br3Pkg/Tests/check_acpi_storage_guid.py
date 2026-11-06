#!/usr/bin/env python3
"""Validate the selected board ACPI producer against the build's upstream PCD."""
import argparse,hashlib,json,re,uuid
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build-output',type=Path,required=True);p.add_argument('--mde-module-dec',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def raw_bytes(s):
 values=re.findall(r'0x([0-9a-fA-F]+)',s)
 result=bytes(int(x,16) for x in values)
 if len(result)!=16:raise SystemExit('ACPI storage PCD is not a 16-byte GUID')
 return result
# Read the actual selected producer, rather than duplicating the DSC variant matrix.
makefiles=list((a.build_output/'ARM/a04br3Pkg/WindowsArm32/Acpi').glob('*/GNUmakefile'))
if len(makefiles)!=1:raise SystemExit('Expected exactly one selected board ACPI producer')
makefile=makefiles[0];variables=dict(re.findall(r'^([A-Z_]+)\s*=\s*(.*?)\s*$',makefile.read_text(),re.M))
inf=Path(variables['MODULE_DIR'])/variables['MODULE_FILE']
match=re.findall(r'^\s*FILE_GUID\s*=\s*([0-9a-fA-F-]+)\s*$',inf.read_text(),re.M)
if len(match)!=1:raise SystemExit('Selected INF has no unique FILE_GUID')
producer=uuid.UUID(match[0]);built_producer=uuid.UUID(variables['MODULE_GUID'])
# Parse the real upstream DEC default in EFI GUID memory byte order.
defaults=[line.split('|')[1] for line in a.mde_module_dec.read_text().splitlines()
          if re.match(r'\s*gEfiMdeModulePkgTokenSpaceGuid\.PcdAcpiTableStorageFile\|',line)]
if len(defaults)!=1:raise SystemExit('Upstream DEC has no unique ACPI storage default')
default=raw_bytes(defaults[0]);loader_guid=uuid.UUID(bytes_le=default)
if producer.bytes_le!=default or built_producer.bytes_le!=default:
 raise SystemExit(f'ACPI storage GUID mismatch: INF={producer}, built={built_producer}, upstream PCD={loader_guid}')
loader=a.build_output/'ARM/MdeModulePkg/Universal/Acpi/AcpiPlatformDxe/AcpiPlatformDxe/DEBUG/AutoGen.c'
compiled=re.findall(r'_gPcd_FixedAtBuild_PcdAcpiTableStorageFile\s*\[16\]\s*=\s*\{([^}]+)\}',loader.read_text())
if len(compiled)!=1 or raw_bytes(compiled[0])!=default:
 raise SystemExit('Compiled AcpiPlatformDxe PCD differs from the upstream default')
ffs=a.build_output/'FV/Ffs'
present=[]
for path in ffs.iterdir():
 try:present.append(uuid.UUID(path.name[:36]))
 except ValueError:pass
if present.count(loader_guid)!=1:raise SystemExit('Loader-selected ACPI FFS missing or duplicated')
result={'status':'host-storage-guid-verified-not-device-validated','selected_inf':str(inf),'selected_inf_sha256':sha(inf),
 'generated_makefile':str(makefile),'built_file_guid':str(built_producer),'upstream_dec':str(a.mde_module_dec),
 'upstream_dec_sha256':sha(a.mde_module_dec),'upstream_pcd_bytes_le':default.hex(),'loader_guid':str(loader_guid),
 'loader_autogen':str(loader),'loader_autogen_sha256':sha(loader),'matching_ffs_count':present.count(loader_guid)}
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n')
print(f'ACPI producer/DEC bytes_le/compiled loader/FFS match: {loader_guid}')
