#!/usr/bin/env python3
"""Build a04br3 FD from clean source and record its actual build provenance."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, sys, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[4]
PATCHES=('AsmMacroLib.patch','Auth-Service.patch','Boot-Manager.patch','Timer.patch','Usb-Bus.patch')
# DeviceBuild.py declares these three as the required build submodules. Its
# a04br3 package path does not include Silicium-ACPI or vendor binaries.
SUBMODULES=('Common/Mu','Common/Mu_OEM_Sample','Mu_Basecore')
def command(*a,env=None):return subprocess.check_output(list(map(str,a)),env=env,text=True).strip()
def git(path,*args,env=None):return command('git','-C',path,*args,env=env)
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
def clean(path):
 if git(path,'status','--porcelain','--untracked-files=all','--ignore-submodules=all'):raise RuntimeError('unclean source: '+str(path))
def patched_tree(base, expected):
 # A private temporary index avoids touching the source checkout's index.
 with tempfile.TemporaryDirectory() as temp:
  env=dict(os.environ,GIT_INDEX_FILE=str(pathlib.Path(temp)/'index'))
  git(base,'read-tree','HEAD',env=env)
  if expected:
   for name in PATCHES:subprocess.run(['git','-C',str(base),'apply','--cached',str(ROOT/'Resources/MuPatches'/name)],env=env,check=True)
  else:subprocess.run(['git','-C',str(base),'add','-u'],env=env,check=True)
  return git(base,'write-tree',env=env)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--unixnu',required=True,type=pathlib.Path);p.add_argument('--out',required=True,type=pathlib.Path);args=p.parse_args()
 u=args.unixnu.resolve(strict=True);out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
 fd=ROOT/'Build/a04br3Pkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd'
 if fd.exists() or (out/'SILICIUM_UEFI.fd').exists() or (out/'manifest.json').exists():raise RuntimeError('refusing existing output')
 clean(ROOT);clean(u)
 source=git(ROOT,'rev-parse','HEAD');unixnu=git(u,'rev-parse','HEAD')
 refs={}
 for name in SUBMODULES:
  sub=ROOT/name; expected=git(ROOT,'ls-tree','HEAD',name).split()[2]
  if pathlib.Path(git(sub,'rev-parse','--show-toplevel')).resolve()!=sub.resolve():raise RuntimeError('submodule not initialized: '+name)
  if git(sub,'rev-parse','HEAD')!=expected:raise RuntimeError('submodule mismatch: '+name)
  if name != 'Mu_Basecore':clean(sub)
  refs[name]=expected
 darwin=u/'drivers/DarwinBoot'
 if pathlib.Path(git(darwin,'rev-parse','--show-toplevel')).resolve()!=darwin.resolve():raise RuntimeError('DarwinBoot submodule not initialized')
 clean(darwin);darwin_rev=git(darwin,'rev-parse','HEAD')
 if darwin_rev!=git(u,'ls-tree','HEAD','drivers/DarwinBoot').split()[2]:raise RuntimeError('DarwinBoot gitlink mismatch')
 patch_hash={name:sha(ROOT/'Resources/MuPatches'/name) for name in PATCHES}
 base=ROOT/'Mu_Basecore'
 if git(base,'diff','--cached','--name-only') or git(base,'ls-files','--others','--exclude-standard'):
  raise RuntimeError('Mu_Basecore has staged or untracked changes')
 prior_diff=command('git','-C',base,'diff','--binary')
 if prior_diff and patched_tree(base,False)!=patched_tree(base,True):
  raise RuntimeError('Mu_Basecore differs from recorded patches')
 stamp=git(ROOT,'log','-1','--format=%ct')
 env=dict(os.environ,UNIXNU_ROOT=str(u),SOURCE_DATE_EPOCH=stamp,A04BR3_TEST_CYCLE='1',A04BR3_CPU_STAGE_DIAG='1',A04BR3_CHARGER_OWNER='1')
 cmd=[sys.executable,str(ROOT/'build_uefi.py'),'-d','a04br3','-r','DEBUG','-c']
 try:
  subprocess.run(cmd,cwd=ROOT,env=env,check=True)
 finally:
  # The upstream builder reverses these patches before setup. If it exits
  # early in that clean state, restore the exact pre-existing patch set.
  if prior_diff and not command('git','-C',base,'diff','--binary'):
   for name in PATCHES:subprocess.run(['git','-C',str(base),'apply',str(ROOT/'Resources/MuPatches'/name)],check=True)
 if not fd.is_file() or fd.stat().st_size!=0x300000:raise RuntimeError('missing or unexpected FD size')
 if git(ROOT,'rev-parse','HEAD')!=source or git(u,'rev-parse','HEAD')!=unixnu:raise RuntimeError('source changed during build')
 for name in SUBMODULES:
  if git(ROOT/name,'rev-parse','HEAD')!=refs[name]:raise RuntimeError('submodule changed: '+name)
  if name!='Mu_Basecore':clean(ROOT/name)
 base=ROOT/'Mu_Basecore';actual=patched_tree(base,False);expected=patched_tree(base,True)
 if actual!=expected:raise RuntimeError('Mu_Basecore differs from recorded patches')
 if prior_diff and command('git','-C',base,'diff','--binary')!=prior_diff:
  raise RuntimeError('preexisting Mu_Basecore patch state changed')
 shutil.copyfile(fd,out/'SILICIUM_UEFI.fd')
 manifest={'schema':1,'uefi_commit':source,'unixnu_commit':unixnu,'darwinboot_commit':darwin_rev,'submodules':refs,'unused_submodules':['Silicium-ACPI','Binaries','Silicon/Silicium/OpensslPkg/Library/OpensslLib/openssl'],'mu_basecore_patched_tree':actual,'patch_sha256':patch_hash,'source_date_epoch':stamp,'target':'DEBUG','toolchain':'CLANGPDB','defines':{'A04BR3_TEST_CYCLE':'1','A04BR3_CPU_STAGE_DIAG':'1','A04BR3_CHARGER_OWNER':'1'},'command':['python3','build_uefi.py','-d','a04br3','-r','DEBUG','-c'],'fd_sha256':sha(out/'SILICIUM_UEFI.fd'),'fd_bytes':fd.stat().st_size,'clang_version':command('clang','--version').splitlines()[0]}
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('recorded FD:',out/'SILICIUM_UEFI.fd')
if __name__=='__main__':main()
