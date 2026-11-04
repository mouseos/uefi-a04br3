#!/usr/bin/env bash
# Build-only diagnostic. Never flash: runtime and disk safety remain unverified.
set -euo pipefail
if [[ $# != 5 ]]; then
  echo 'usage: build-diagnostic.sh CANONICAL_UEFI_ROOT EDK2_TOOLS_ROOT CONF_TEMPLATE ARM_GCC_PREFIX OUTPUT_NAME' >&2; exit 2
fi
canonical=$(realpath "$1"); oldtools=$(realpath "$2"); template=$(realpath "$3"); gccprefix=$4; output=$5
three=${A04_DIAGNOSTIC_THREE_CPU:-0}
a64prefix=${A04_AARCH64_PREFIX:-aarch64-linux-gnu-}
[[ $three == 0 || $three == 1 ]] || exit 2
[[ $output =~ ^[A-Za-z0-9_-]+$ ]] || exit 2
root=$(git rev-parse --show-toplevel)
[[ -z $(git -C "$root" -c core.filemode=false status --porcelain --untracked-files=normal --ignore-submodules=all) ]] || { echo 'source worktree must be clean' >&2; exit 2; }
[[ ! -e "$root/Build/$output" ]] || { echo 'output already exists' >&2; exit 2; }
for f in "$template"/{target.txt,tools_def.txt,build_rule.txt} "$oldtools/ArmPkg/Library/GccLto/liblto-arm.a" "$oldtools/BaseTools/Source/C/bin/GenFw" "${gccprefix}gcc"; do [[ -f $f ]] || { echo "missing input $f" >&2; exit 2; }; done
conf=$(mktemp -d "${TMPDIR:-/tmp}/a04-windows-build.XXXXXXXX")
trap 'rm -r -- "$conf"' EXIT
cp "$template"/{target.txt,tools_def.txt,build_rule.txt} "$conf/"
python3 - "$conf/tools_def.txt" "$root" "$oldtools" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text(); root=Path(sys.argv[2]); old=Path(sys.argv[3])
needle='DEFINE GCC5_ARM_CC_FLAGS             = DEF(GCC49_ARM_CC_FLAGS)'
assert s.count(needle)==1
inc=root/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Include'
board=root/'Platforms/Benesse/a04br3Pkg/Include'
s=s.replace(needle, f'DEFINE GCC5_ARM_CC_FLAGS             = -I{inc} -I{inc}/Arm -I{board} -DEFI_REMOVABLE_MEDIA_FILE_NAME=EFI_REMOVABLE_MEDIA_FILE_NAME_EBC -include {inc}/Uefi/UefiBaseType.h DEF(GCC49_ARM_CC_FLAGS)')
s=s.replace('*_GCC5_ARM_PP_FLAGS              = $(PLATFORM_FLAGS) DEF(GCC_PP_FLAGS)', '*_GCC5_ARM_PP_FLAGS              = -DEFI_REMOVABLE_MEDIA_FILE_NAME=EFI_REMOVABLE_MEDIA_FILE_NAME_EBC -D__UEFI_BASETYPE_H__ $(PLATFORM_FLAGS) DEF(GCC_PP_FLAGS)')
s=s.replace('-L$(WORKSPACE)/ArmPkg/Library/GccLto',f'-L{old}/ArmPkg/Library/GccLto')
s=s.replace('DEBUG_GCC5_ARM_CC_FLAGS        = DEF(GCC5_ARM_CC_FLAGS) -flto -Wno-unused-but-set-variable -Wno-unused-const-variable','DEBUG_GCC5_ARM_CC_FLAGS        = DEF(GCC5_ARM_CC_FLAGS) -flto -Wno-unused-but-set-variable -Wno-unused-const-variable -lgcc')
p.write_text(s)
PY
export WORKSPACE=$root EDK_TOOLS_PATH="$canonical/Mu_Basecore/BaseTools" CONF_PATH=$conf
export PACKAGES_PATH="$root/Platforms/Benesse:$root/Silicon/MediaTek:$root/Silicon/Silicium:$canonical/Mu_Basecore:$canonical/Common/Mu:$canonical/Common/Mu_OEM_Sample"
export GCC5_ARM_PREFIX=$gccprefix PYTHON_COMMAND=python3 SOURCE_DATE_EPOCH
SOURCE_DATE_EPOCH=$(git -C "$root" log -1 --format=%ct)
export PATH="$oldtools/BaseTools/Source/C/bin:$EDK_TOOLS_PATH/BinWrappers/PosixLike:$PATH"
buildflags=()
if [[ $three == 1 ]]; then
  buildflags=(-D A04BR3_EXPERIMENTAL_THREE_CPU=1 -D A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT=1)
fi
"$EDK_TOOLS_PATH/BinWrappers/PosixLike/build" -p a04br3Pkg/WindowsArm32/WindowsArm32.dsc -a ARM -b DEBUG -t GCC5 -D FD_BASE=0x41080000 -D FD_SIZE=0x00300000 -D FD_BLOCKS=0x300 -D "A04_BUILD_OUTPUT=Build/$output" "${buildflags[@]}"
if [[ $three == 1 ]]; then
  fv="$root/Build/$output/DEBUG_GCC5/FV"
  "$a64prefix"gcc -c "$root/Platforms/Benesse/a04br3Pkg/WindowsArm32/BootShim/BootShim.S" -o "$fv/BootShim.o" \
    -DUEFI_BASE=0x41080000 -DUEFI_SIZE=0x300000 -DA04BR3_A64_CPU_TEST=1 -DA04BR3_A32_SECONDARY_TEST=1 \
    -DA04BR3_PARK_TEST=1 -DA04BR3_SMP_TEST=1 -DA04BR3_WDT_HANDOFF_TEST=0 -DA04BR3_FOUR_CPU_TEST=0 \
    -DA04BR3_A64_WDT_TEST=1 -DA04BR3_A64_WDT_SECONDS=420 -DA04BR3_KERNEL_MARKER=0 -DA04BR3_FB_MONITOR=0 \
    -DA04BR3_OFFLINE_WDT_UNBOUNDED=1 -DA04BR3_WORKER_NO_UART_RESET=1
  "$a64prefix"objcopy -O binary "$fv/BootShim.o" "$fv/BootShim.bin"
fi
python3 - "$root" "$canonical" "$oldtools" "$template" "$gccprefix" "$output" "$conf" "$three" "$a64prefix" <<'PY'
from pathlib import Path
import hashlib,json,subprocess,sys
root,canonical,old,template,gccprefix,output,conf,three,a64prefix=sys.argv[1:]
def sha(path):
 h=hashlib.sha256()
 with open(path,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def git(path,*args): return subprocess.check_output(['git','-C',path,*args]).decode().strip()
fd=Path(root)/'Build'/output/'DEBUG_GCC5/FV/SILICIUM_UEFI.fd'
if not fd.is_file() or fd.stat().st_size != 0x300000: raise SystemExit('FD missing or unexpected size')
mu=Path(canonical)/'Mu_Basecore'
diff=subprocess.check_output(['git','-C',str(mu),'-c','core.filemode=false','diff','--binary'])
submodules=subprocess.check_output(['git','-C',str(mu),'-c','core.filemode=false','submodule','status'],text=True).splitlines()
if diff or any(not x.startswith(' ') for x in submodules):
 raise SystemExit('Mu checkout or nested submodules differ from pinned commits')
other={}
for name in ('Mu','Mu_OEM_Sample'):
 path=Path(canonical)/'Common'/name
 status=subprocess.check_output(['git','-C',str(path),'-c','core.filemode=false','status','--porcelain','--untracked-files=normal'],text=True)
 if status: raise SystemExit(f'dirty dependency: {name}')
 other[name]=git(str(path),'rev-parse','HEAD')
tools=Path(old)/'BaseTools/Source/C/bin'
inputs=[Path(template)/n for n in ('target.txt','tools_def.txt','build_rule.txt')]
inputs += [Path(conf)/'tools_def.txt', Path(old)/'ArmPkg/Library/GccLto/liblto-arm.a']
inputs += [tools/n for n in ('GenFw','GenFfs','GenFv','GenSec','LzmaCompress')]
inputs += [Path(gccprefix+n) for n in ('gcc','gcc-ar','as','ld','objcopy')]
libgcc=subprocess.check_output([gccprefix+'gcc','-march=armv7-a','-mthumb','-mfloat-abi=soft','-print-libgcc-file-name'],text=True).strip()
inputs.append(Path(libgcc))
result={'status':'diagnostic-only-not-for-device','source_commit':git(root,'rev-parse','HEAD'),
 'mu_commit':git(str(mu),'rev-parse','HEAD'),'mu_uncommitted_diff_sha256':hashlib.sha256(diff).hexdigest(),'mu_submodules':submodules,'other_dependencies':other,
 'source_date_epoch':git(root,'log','-1','--format=%ct'),'architecture':'ARM','target':'DEBUG','toolchain':'GCC5',
 'build_defines':{'FD_BASE':'0x41080000','FD_SIZE':'0x00300000','FD_BLOCKS':'0x300'},
 'inputs':{str(p):sha(p) for p in inputs},'fd_sha256':sha(fd),'fd_bytes':fd.stat().st_size,
 'limitations':['single CPU only','watchdog worker and parking not connected','RP attribute unsupported','MMU runtime, cache and NVS unverified','libgcc ABI warnings unreviewed']}
if three == '1':
 import shutil,struct
 obj=fd.parent/'BootShim.o'; shim=fd.parent/'BootShim.bin'; data=shim.read_bytes()
 rel=subprocess.check_output([a64prefix+'readelf','-r',str(obj)],text=True)
 if 'There are no relocations' not in rel: raise SystemExit('BootShim contains relocations')
 nm=subprocess.check_output([a64prefix+'nm','-n',str(obj)],text=True)
 payload=int(next(x.split()[0] for x in nm.splitlines() if x.endswith(' _Payload')),16)
 if payload!=len(data) or struct.unpack_from('<QQ',data,0x48)!=(0x41080000,0x300000):
  raise SystemExit('BootShim payload or FD layout mismatch')
 combined=fd.parent/'SILICIUM_UEFI.fd.shimmed'
 with combined.open('wb') as dest, fd.open('rb') as src:
  dest.write(data); shutil.copyfileobj(src,dest)
 names=('gcc','objcopy','readelf','nm')
 result['aarch64_tools']={n:sha(Path(shutil.which(a64prefix+n) or a64prefix+n)) for n in names}
 result['build_defines'].update(A04BR3_EXPERIMENTAL_THREE_CPU='1',A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT='1')
 result['shim_defines']={'UEFI_BASE':'0x41080000','UEFI_SIZE':'0x300000','A04BR3_A64_CPU_TEST':1,
  'A04BR3_A32_SECONDARY_TEST':1,'A04BR3_PARK_TEST':1,'A04BR3_SMP_TEST':1,'A04BR3_WDT_HANDOFF_TEST':0,
  'A04BR3_FOUR_CPU_TEST':0,'A04BR3_A64_WDT_TEST':1,'A04BR3_A64_WDT_SECONDS':420,'A04BR3_KERNEL_MARKER':0,
  'A04BR3_FB_MONITOR':0,'A04BR3_OFFLINE_WDT_UNBOUNDED':1,'A04BR3_WORKER_NO_UART_RESET':1}
 result.update(shim_sha256=sha(shim),shim_bytes=len(data),shimmed_fd_sha256=sha(combined),shimmed_fd_bytes=combined.stat().st_size)
 result['limitations']=['experimental three CPU; no runtime validation','worker heartbeat is not proof of watchdog operation',
  'other reset sources not excluded','DISM forbidden','RP attribute unsupported',
  'MMU runtime, cache and NVS unverified','libgcc ABI warnings unreviewed']
(fd.parent/'diagnostic-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
PY
