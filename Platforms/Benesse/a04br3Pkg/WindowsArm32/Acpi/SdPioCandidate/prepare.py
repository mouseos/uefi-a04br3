#!/usr/bin/env python3
"""Compile a pinned, unpublished SD ACPI candidate; never edit/build canonical."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parent
repo=source.parents[5];task=repo.parents[1]
out=a.out.resolve();out.relative_to(task)
if out.exists():raise SystemExit('Output must be a new directory')
if subprocess.check_output(['git','-C',str(repo),'status','--porcelain','--ignore-submodules=all'],text=True).strip():
 raise SystemExit('Commit the board source before preparing provenance')
commit='f729a77d73dc84894e7e543ade2b20e89c5ab880'
driver=task/'windows-rt/a04br3-windows-drivers';path='drivers/storage/mt8167-sd/acpi/MT8167-SD.asl'
ref=subprocess.check_output(['git','-C',str(driver),'show',commit+':'+path])
assert ref==(source/'MT8167-SD.asl').read_bytes(),'Pinned Windows ACPI source changed'
compiler=Path(shutil.which('iasl')).resolve();out.mkdir(parents=True)
with (out/'iasl.log').open('w') as log:
 subprocess.run([str(compiler),'-p',str(out/'MT8167-SD'),str(source/'MT8167-SD.asl')],stdout=log,stderr=subprocess.STDOUT,check=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
manifest={'status':'compiled-unpublished-SD-candidate-not-PnP-or-power-validated','uefi_source_commit':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),'source_dirty':False,'helper_sha256':sha(Path(__file__)),'driver_commit':commit,'driver_acpi_source':path,'driver_source_bytes_match':True,'source_sha256':sha(source/'MT8167-SD.asl'),'aml_sha256':sha(out/'MT8167-SD.aml'),'iasl_sha256':sha(compiler),'published_by_default':False,'dsc_fdf_connected':False,'d0_rail_clock_owner_qualified':False,'rtc_sd_combined_flash_allowed':False,'sd_only_required_rtc_defines':{'A04BR3_MT6392_RTC_READ':'0','A04BR3_MT6392_RTC_RUNTIME_COUNTER':'0'},'fd_generated':False,'boot_generated':False,'device_access':False}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
