#!/usr/bin/env bash
# Build-only diagnostic. Never flash: runtime and disk safety remain unverified.
set -euo pipefail
if [[ $# != 5 ]]; then
  echo 'usage: build-diagnostic.sh CANONICAL_UEFI_ROOT EDK2_TOOLS_ROOT CONF_TEMPLATE ARM_GCC_PREFIX OUTPUT_NAME' >&2; exit 2
fi
canonical=$(realpath "$1"); oldtools=$(realpath "$2"); template=$(realpath "$3"); gccprefix=$4; output=$5
three=${A04_DIAGNOSTIC_THREE_CPU:-0}
cpu1_l1=${A04_DIAGNOSTIC_FOUR_CPU_L1_CLEAN:-0}
battery=${A04_DIAGNOSTIC_BATTERY:-0}
gpu=${A04_DIAGNOSTIC_GPU:-0}
sd_pio=${A04_DIAGNOSTIC_SD_PIO_PROFILE:-0}
gpu_domain=${A04_DIAGNOSTIC_GPU_DOMAIN_OWNER:-0}
gpu_thermal=${A04_DIAGNOSTIC_GPU_THERMAL_PROFILE:-0}
[[ $gpu_thermal == 0 || $gpu_thermal == 1 ]] || exit 2
[[ $gpu_domain == 0 || $gpu_domain == 1 ]] || exit 2
[[ $sd_pio == 0 || $sd_pio == 1 ]] || exit 2
if [[ $sd_pio == 1 ]]; then
 [[ ${A04_DIAGNOSTIC_MT6392_RTC_READ:-0} == 0 && ${A04_DIAGNOSTIC_MT6392_RTC_RUNTIME_COUNTER:-0} == 0 ]] || { echo 'SD-only excludes RTC candidates' >&2; exit 2; }
fi
gpu_mmpll=${A04_DIAGNOSTIC_GPU_MMPLL:-0}
[[ $gpu_mmpll == 0 || ($gpu_mmpll == 1 && $gpu == 1) ]] || exit 2
display=${A04_DIAGNOSTIC_DISPLAY:-0}
clear_lk_overlay=${A04_DIAGNOSTIC_CLEAR_LK_OVERLAY:-0}
[[ $clear_lk_overlay == 0 || $clear_lk_overlay == 1 ]] || exit 2
display_gce=${A04_DIAGNOSTIC_DISPLAY_GCE_RESOURCES:-0}
unified_display=${A04_DIAGNOSTIC_UNIFIED_DISPLAY356:-0}
panel_timing=${A04_DIAGNOSTIC_PANEL_TIMING_HANDOFF:-0}
[[ $panel_timing == 0 || ($panel_timing == 1 && $display == 1 && $unified_display == 1) ]] || exit 2
[[ $unified_display == 0 || ($unified_display == 1 && $display == 1 && $display_gce == 0) ]] || exit 2
display_brightness=${A04_DIAGNOSTIC_DISPLAY_BRIGHTNESS_RESOURCES:-0}
touch=${A04_DIAGNOSTIC_TOUCH:-0}
shared=${A04_DIAGNOSTIC_SHARED:-0}
shared_touch_power=${A04_DIAGNOSTIC_SHARED_TOUCH_POWER:-0}
apmixed=${A04_DIAGNOSTIC_SHARED_APMIXED:-0}
shared_io=${A04_DIAGNOSTIC_SHARED_IO_PROFILE:-0}
[[ $sd_pio == 0 || $shared_io == 1 ]] || exit 2
[[ $gpu_domain == 0 || ($sd_pio == 1 && $shared_io == 1) ]] || exit 2
[[ $shared_io == 0 || ($shared_io == 1 && $shared == 1 && $apmixed == 1 && $battery == 1 && $gpu == 1 && $display == 1 && $shared_touch_power == 1) ]] || exit 2
bq=${A04_DIAGNOSTIC_BQ_HANDOFF:-0}
thermal=${A04_DIAGNOSTIC_THERMAL_HANDOFF_RECORD:-0}
thermal_pio=${A04_DIAGNOSTIC_THERMAL_PIO_HANDOFF:-0}
[[ $thermal_pio == 0 || ($thermal_pio == 1 && $thermal == 1) ]] || exit 2
[[ $gpu_thermal == 0 || ($gpu_domain == 1 && $sd_pio == 1 && $thermal_pio == 1) ]] || { echo "1101 requires GPU domain/SD1/thermal PIO handoff" >&2; exit 2; }
gce_observation=${A04_DIAGNOSTIC_GCE_OBSERVATION:-0}
[[ $gce_observation == 0 || $gce_observation == 1 ]] || exit 2
[[ $thermal == 0 || $thermal == 1 ]] || exit 2
[[ $bq == 0 || ($bq == 1 && $battery == 1) ]] || exit 2
[[ $shared == 0 || $shared == 1 ]] || exit 2
[[ $shared_touch_power == 0 || ($shared_touch_power == 1 && $shared == 1) ]] || exit 2
[[ $apmixed == 0 || ($apmixed == 1 && $shared == 1) ]] || exit 2
[[ $shared == 0 || $touch == 0 ]] || exit 2
[[ $touch == 0 || $touch == 1 ]] || exit 2
[[ $display == 0 || ($display == 1 && $gpu == 1) ]] || exit 2
[[ $display_gce == 0 || ($display_gce == 1 && $display == 1) ]] || exit 2
[[ $display_brightness == 0 || ($display_brightness == 1 && $display == 1) ]] || exit 2
[[ $gpu == 0 || $gpu == 1 ]] || exit 2
[[ $battery == 0 || $battery == 1 ]] || exit 2
a64prefix=${A04_AARCH64_PREFIX:-aarch64-linux-gnu-}
overlay=${A04_LIBGCC_OVERLAY:-}
[[ $three == 0 || $three == 1 || $three == 2 || $three == 3 || $three == 4 || $three == 4 ]] || exit 2
[[ $cpu1_l1 == 0 || ($cpu1_l1 == 1 && $three == 4) ]] || exit 2
if [[ -n $overlay ]]; then
  overlay=$(realpath "$overlay")
  [[ $three != 0 && $overlay != *[[:space:]]* && -f $overlay/libgcc.a && -f $overlay/manifest.json ]] || exit 2
  python3 - "$overlay" "$gccprefix" <<'PY'
import hashlib,json,subprocess,sys
from pathlib import Path
path=Path(sys.argv[1]); prefix=sys.argv[2]
def sha(p):
 with open(p,'rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
m=json.loads((path/'manifest.json').read_text())
original=subprocess.check_output([prefix+'gcc','-march=armv7-a','-mthumb','-mfloat-abi=soft','-print-libgcc-file-name'],text=True).strip()
if (m.get('status')!='experimental-not-integrated' or m.get('overlay_libgcc_sha256')!=sha(path/'libgcc.a') or
    m.get('original_libgcc_sha256')!=sha(original) or m.get('gcc_sha256')!=sha(prefix+'gcc')):
 raise SystemExit('libgcc overlay manifest does not match the inputs')
PY
fi
[[ $output =~ ^[A-Za-z0-9_-]+$ ]] || exit 2
root=$(git rev-parse --show-toplevel)
[[ -z $(git -C "$root" -c core.filemode=false status --porcelain --untracked-files=normal --ignore-submodules=all) ]] || { echo 'source worktree must be clean' >&2; exit 2; }
if [[ -e "$root/Build/$output" ]]; then
  [[ $unified_display == 1 && -n ${A04_DIAGNOSTIC_CACHED_BUILD_PROOF:-} ]] || { echo 'output already exists' >&2; exit 2; }
  python3 - "$root/Build/$output" "$A04_DIAGNOSTIC_CACHED_BUILD_PROOF" <<'CACHEPY'
import hashlib,json,sys
from pathlib import Path
build=Path(sys.argv[1]).resolve();proof=Path(sys.argv[2]);p=json.loads(proof.read_text())
assert p['baseline_source']=='b4ce1992af66adcefa02e34357d5dbf4313eaca2'
assert p['destination']==str(build) and p['role'] in ('verified-existing-1117-build-cache-not-final356','verified356-partial-build-not-final')
assert p['files']
for n,h in p['files'].items():
 f=(build/n).resolve();assert f.is_relative_to(build) and hashlib.sha256(f.read_bytes()).hexdigest()==h,n
CACHEPY
fi
if [[ $gpu == 1 ]]; then
  python3 "$root/Platforms/Benesse/a04br3Pkg/Tests/check_gpu_sysirq_mapping.py"
fi
for f in "$template"/{target.txt,tools_def.txt,build_rule.txt} "$oldtools/ArmPkg/Library/GccLto/liblto-arm.a" "$oldtools/BaseTools/Source/C/bin/GenFw" "${gccprefix}gcc"; do [[ -f $f ]] || { echo "missing input $f" >&2; exit 2; }; done
conf=$(mktemp -d "${TMPDIR:-/tmp}/a04-windows-build.XXXXXXXX")
trap 'rm -r -- "$conf"' EXIT
cp "$template"/{target.txt,tools_def.txt,build_rule.txt} "$conf/"
if [[ $panel_timing == 1 ]]; then
  panel="$root/Platforms/Benesse/a04br3Pkg/WindowsArm32/Drivers/PanelTimingDxe"
  python3 "$panel/generate-aml.py" --output "$conf/PanelTimingAml.h"
  cmp "$conf/PanelTimingAml.h" "$panel/PanelTimingAml.h"
fi
python3 - "$conf/tools_def.txt" "$root" "$oldtools" "$overlay" "$display_gce" "$display_brightness" "$unified_display" <<'PY'
from pathlib import Path
import sys,re
p=Path(sys.argv[1]); s=p.read_text(); root=Path(sys.argv[2]); old=Path(sys.argv[3])
if sys.argv[5]=='1':
 s,n=re.subn(r'(?m)^(DEFINE GCC_ASLPP_FLAGS\s*=\s*[^\r\n]+)',r'\1 -DA04BR3_DISPLAY_GCE_RESOURCES=1',s)
 assert n==1
if sys.argv[6]=='1':
 s,n=re.subn(r'(?m)^(DEFINE GCC_ASLPP_FLAGS\s*=\s*[^\r\n]+)',r'\1 -DA04BR3_DISPLAY_BRIGHTNESS_RESOURCES=1',s)
 assert n==1
if sys.argv[7]=='1':
 s,n=re.subn(r'(?m)^(DEFINE GCC_ASLPP_FLAGS\s*=\s*[^\r\n]+)',r'\1 -DA04BR3_UNIFIED_DISPLAY356=1',s)
 assert n==1
needle='DEFINE GCC5_ARM_CC_FLAGS             = DEF(GCC49_ARM_CC_FLAGS)'
assert s.count(needle)==1
inc=root/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Include'
board=root/'Platforms/Benesse/a04br3Pkg/Include'
s=s.replace(needle, f'DEFINE GCC5_ARM_CC_FLAGS             = -I{inc} -I{inc}/Arm -I{board} -DEFI_REMOVABLE_MEDIA_FILE_NAME=EFI_REMOVABLE_MEDIA_FILE_NAME_EBC -include {inc}/Uefi/UefiBaseType.h DEF(GCC49_ARM_CC_FLAGS)')
s=s.replace('*_GCC5_ARM_PP_FLAGS              = $(PLATFORM_FLAGS) DEF(GCC_PP_FLAGS)', '*_GCC5_ARM_PP_FLAGS              = -DEFI_REMOVABLE_MEDIA_FILE_NAME=EFI_REMOVABLE_MEDIA_FILE_NAME_EBC -D__UEFI_BASETYPE_H__ $(PLATFORM_FLAGS) DEF(GCC_PP_FLAGS)')
s=s.replace('-L$(WORKSPACE)/ArmPkg/Library/GccLto',f'-L{old}/ArmPkg/Library/GccLto')
s=s.replace('DEBUG_GCC5_ARM_CC_FLAGS        = DEF(GCC5_ARM_CC_FLAGS) -flto -Wno-unused-but-set-variable -Wno-unused-const-variable','DEBUG_GCC5_ARM_CC_FLAGS        = DEF(GCC5_ARM_CC_FLAGS) -flto -Wno-unused-but-set-variable -Wno-unused-const-variable -lgcc')
if sys.argv[4]:
 needle='DEFINE GCC5_ARM_DLINK_FLAGS          = DEF(GCC49_ARM_DLINK_FLAGS)'
 assert s.count(needle)==1
 s=s.replace(needle, f'DEFINE GCC5_ARM_DLINK_FLAGS          = -L{sys.argv[4]} DEF(GCC49_ARM_DLINK_FLAGS)')
p.write_text(s)
PY
export WORKSPACE=$root EDK_TOOLS_PATH="$canonical/Mu_Basecore/BaseTools" CONF_PATH=$conf
export PACKAGES_PATH="$root/Platforms/Benesse:$root/Silicon/MediaTek:$root/Silicon/Silicium:$canonical/Mu_Basecore:$canonical/Common/Mu:$canonical/Common/Mu_OEM_Sample"
export GCC5_ARM_PREFIX=$gccprefix PYTHON_COMMAND=python3 SOURCE_DATE_EPOCH
SOURCE_DATE_EPOCH=$(git -C "$root" log -1 --format=%ct)
export PATH="$oldtools/BaseTools/Source/C/bin:$EDK_TOOLS_PATH/BinWrappers/PosixLike:$PATH"
buildflags=()
if [[ $three != 0 ]]; then
  buildflags=(-D A04BR3_EXPERIMENTAL_THREE_CPU=1 -D A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT=1)
fi
if [[ $three == 2 ]]; then buildflags+=(-D A04BR3_BOOT_DIAGNOSTIC=1); fi
if [[ $three == 3 || $three == 4 ]]; then buildflags+=(-D A04BR3_CACHE_TEST=1); fi
if [[ $three == 4 ]]; then buildflags+=(-D A04BR3_FOUR_CPU_TEST=1); fi
if [[ $three == 2 || $three == 3 || $three == 4 ]]; then buildflags+=(-D A04BR3_BOUNDED_RECOVERY=1); fi
if [[ $battery == 1 ]]; then buildflags+=(-D A04BR3_BATTERY_PROVIDER=1); fi
if [[ $bq == 1 ]]; then buildflags+=(-D A04BR3_BQ_HANDOFF=1); fi
if [[ $thermal == 1 ]]; then buildflags+=(-D A04BR3_THERMAL_HANDOFF_RECORD=1); fi
if [[ $gce_observation == 1 ]]; then buildflags+=(-D A04BR3_GCE_OBSERVATION=1); fi
if [[ $gpu_mmpll == 1 ]]; then buildflags+=(-D A04BR3_GPU_MMPLL=1); fi
if [[ $gpu == 1 ]]; then buildflags+=(-D A04BR3_GPU_HANDOFF=1); fi
if [[ $display == 1 ]]; then buildflags+=(-D A04BR3_DISPLAY_PROVIDER=1); fi
if [[ $clear_lk_overlay == 1 ]]; then buildflags+=(-D A04BR3_CLEAR_LK_OVERLAY=1); fi
if [[ $touch == 1 ]]; then buildflags+=(-D A04BR3_TOUCH_PROVIDER=1); fi
if [[ $shared == 1 ]]; then buildflags+=(-D A04BR3_SHARED_PARENT=1); fi
if [[ $shared_touch_power == 1 ]]; then buildflags+=(-D A04BR3_SHARED_TOUCH_POWER=1); fi
if [[ $thermal_pio == 1 ]]; then buildflags+=(-D A04BR3_THERMAL_PIO_HANDOFF=1); fi
if [[ $sd_pio == 1 ]]; then buildflags+=(-D A04BR3_SD_PIO_PROFILE=1); fi
if [[ $gpu_domain == 1 ]]; then buildflags+=(-D A04BR3_GPU_DOMAIN_OWNER=1); fi
if [[ $gpu_thermal == 1 ]]; then buildflags+=(-D A04BR3_GPU_THERMAL_PROFILE=1); fi
if [[ $unified_display == 1 ]]; then buildflags+=(-D A04BR3_UNIFIED_DISPLAY356=1); fi
if [[ $panel_timing == 1 ]]; then buildflags+=(-D A04BR3_PANEL_TIMING_HANDOFF=1); fi
if [[ $shared_io == 1 ]]; then buildflags+=(-D A04BR3_SHARED_IO_PROFILE=1); fi
if [[ $apmixed == 1 ]]; then buildflags+=(-D A04BR3_SHARED_APMIXED=1); fi
"$EDK_TOOLS_PATH/BinWrappers/PosixLike/build" -p a04br3Pkg/WindowsArm32/WindowsArm32.dsc -a ARM -b DEBUG -t GCC5 -D FD_BASE=0x41080000 -D FD_SIZE=0x00300000 -D FD_BLOCKS=0x300 -D "A04_BUILD_OUTPUT=Build/$output" "${buildflags[@]}"
python3 "$root/Platforms/Benesse/a04br3Pkg/Tests/check_acpi_storage_guid.py" \
  --build-output "$root/Build/$output/DEBUG_GCC5" \
  --mde-module-dec "$canonical/Mu_Basecore/MdeModulePkg/MdeModulePkg.dec" \
  --out "$root/Build/$output/DEBUG_GCC5/FV/acpi-storage-validation.json"
if [[ $three != 0 ]]; then
  fv="$root/Build/$output/DEBUG_GCC5/FV"
  offline=1; no_uart=1; seconds=420
  if [[ $three == 2 || $three == 3 || $three == 4 ]]; then offline=0; no_uart=0; fi
  if [[ $three == 2 ]]; then seconds=120; fi
  guard=0; if [[ $three == 2 || $three == 3 || $three == 4 ]]; then guard=1; fi
  framebuffer=0; if [[ $three == 3 || $three == 4 ]]; then framebuffer=1; fi
  four=0; if [[ $three == 4 ]]; then four=1; fi
  "$a64prefix"gcc -c "$root/Platforms/Benesse/a04br3Pkg/WindowsArm32/BootShim/BootShim.S" -o "$fv/BootShim.o" \
    -DUEFI_BASE=0x41080000 -DUEFI_SIZE=0x300000 -DA04BR3_A64_CPU_TEST=1 -DA04BR3_A32_SECONDARY_TEST=1 \
    -DA04BR3_PARK_TEST=1 -DA04BR3_SMP_TEST=1 -DA04BR3_WDT_HANDOFF_TEST=0 -DA04BR3_FOUR_CPU_TEST="$four" -DA04BR3_FOUR_CPU_L1_CLEAN="$cpu1_l1" \
    -DA04BR3_A64_WDT_TEST=1 -DA04BR3_A64_WDT_SECONDS="$seconds" -DA04BR3_KERNEL_MARKER="$framebuffer" -DA04BR3_FB_MONITOR="$framebuffer" \
    -DA04BR3_OFFLINE_WDT_UNBOUNDED="$offline" -DA04BR3_WORKER_NO_UART_RESET="$no_uart" -DA04BR3_WORKER_HEARTBEAT=1 \
    -DA04BR3_EARLY_FDT_GUARD="$guard"
  shimobj="$fv/BootShim.o"
  if [[ $guard == 1 ]]; then
    shimdir="$root/Platforms/Benesse/a04br3Pkg/WindowsArm32/BootShim"
    for src in FdtGuard EarlyRecovery; do
      "$a64prefix"gcc -c -O2 -ffreestanding -fno-stack-protector -fno-builtin -fno-pic -fno-pie \
        -fno-asynchronous-unwind-tables -fno-unwind-tables -mgeneral-regs-only -mstrict-align -DA04_FDT_TRACE=1 "$shimdir/$src.c" -o "$fv/$src.o"
    done
    cat > "$conf/shim.ld" <<'EOF'
SECTIONS { . = 0; .text : { *(.text*) *(.rodata*) . = ALIGN(16); *(.payload*) }
 /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) } }
EOF
    "$a64prefix"ld -T "$conf/shim.ld" -o "$fv/BootShim.linked" "$fv/BootShim.o" "$fv/FdtGuard.o" "$fv/EarlyRecovery.o"
    shimobj="$fv/BootShim.linked"
  fi
  "$a64prefix"objcopy -O binary "$shimobj" "$fv/BootShim.bin"
fi
python3 - "$root" "$canonical" "$oldtools" "$template" "$gccprefix" "$output" "$conf" "$three" "$a64prefix" "$overlay" "$battery" "$gpu" "$display" "$touch" "$shared" "$apmixed" "$bq" "$thermal" "$shared_touch_power" "$display_gce" "$gce_observation" "$cpu1_l1" "$clear_lk_overlay" "$gpu_mmpll" "$shared_io" "$display_brightness" "$sd_pio" "$gpu_domain" "$thermal_pio" "$gpu_thermal" <<'PY'
from pathlib import Path
import hashlib,json,subprocess,sys
root,canonical,old,template,gccprefix,output,conf,three,a64prefix,overlay,battery,gpu,display,touch,shared,apmixed,bq,thermal,shared_touch_power,display_gce,gce_observation,cpu1_l1,clear_lk_overlay,gpu_mmpll,shared_io,display_brightness,sd_pio,gpu_domain,thermal_pio,gpu_thermal=sys.argv[1:]
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
result['acpi_storage_validation']=json.loads((fd.parent/'acpi-storage-validation.json').read_text())
result['build_defines']['A04BR3_BATTERY_PROVIDER']=battery
result['build_defines']['A04BR3_GPU_HANDOFF']=gpu
result['build_defines']['A04BR3_GPU_MMPLL']=gpu_mmpll
result['build_defines']['A04BR3_GPU_DOMAIN_OWNER']=gpu_domain
result['gpu_domain']=dict(enabled=gpu_domain=='1',parent='1014 SD-on V2 SPM',resource_base='0x10006000',resource_bytes=4096,default_off=True)
result['build_defines']['A04BR3_THERMAL_PIO_HANDOFF']=thermal_pio
result['build_defines']['A04BR3_GPU_THERMAL_PROFILE']=gpu_thermal
result['gpu_thermal']=dict(enabled=gpu_thermal=='1',parent='1101 RTC/capture1026 GPU/SD1/thermal PIO',thermal_base='0x1100d000',adc_base='0x11003000',range_bytes=4096,default_off=True)
result['build_defines']['A04BR3_SD_PIO_PROFILE']=sd_pio
result['sd_pio']=dict(json.loads((Path(root)/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Acpi/sd-pio-contract.json').read_text()),enabled=sd_pio=='1')
result['gpu_clock_record']={'name':'Mt8167GpuClock','guid':'9c50bc94-6c80-4fa2-939d-9278d07f7010','version':1,'bytes':44,'attributes':6,'fields':['version','bytes','parent','clock_hz','con0','con1','power','rail_control','rail_status','sel0','sel1'],'publish':'validated-parent7-only','fallback_hz':26000000}
result['build_defines']['A04BR3_DISPLAY_PROVIDER']=display
result['build_defines']['A04BR3_CLEAR_LK_OVERLAY']=clear_lk_overlay
result['build_defines']['A04BR3_DISPLAY_GCE_RESOURCES']=display_gce
result['build_defines']['A04BR3_UNIFIED_DISPLAY356']=__import__('os').environ.get('A04_DIAGNOSTIC_UNIFIED_DISPLAY356','0')
result['build_defines']['A04BR3_PANEL_TIMING_HANDOFF']=__import__('os').environ.get('A04_DIAGNOSTIC_PANEL_TIMING_HANDOFF','0')
result['build_defines']['A04BR3_DISPLAY_BRIGHTNESS_RESOURCES']=display_brightness
result['build_defines']['A04BR3_TOUCH_PROVIDER']=touch
result['build_defines']['A04BR3_SHARED_PARENT']=shared
result['build_defines']['A04BR3_SHARED_TOUCH_POWER']=shared_touch_power
result['build_defines']['A04BR3_SHARED_APMIXED']=apmixed
result['build_defines']['A04BR3_SHARED_IO_PROFILE']=shared_io
if shared_io == '1':
 contract=Path(root)/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Acpi/shared-io-contract.json'
 result['shared_io_contract']={'sha256':sha(contract),'contract':json.loads(contract.read_text())}
 result['limitations'].extend(result['shared_io_contract']['contract']['limitations'])
result['build_defines']['A04BR3_BQ_HANDOFF']=bq
result['build_defines']['A04BR3_THERMAL_HANDOFF_RECORD']=thermal
result['build_defines']['A04BR3_GCE_OBSERVATION']=gce_observation
if three != '0':
 import shutil,struct
 obj=fd.parent/('BootShim.linked' if three in ('2','3','4') else 'BootShim.o'); shim=fd.parent/'BootShim.bin'; data=shim.read_bytes()
 rel=subprocess.check_output([a64prefix+'readelf','-r',str(obj)],text=True)
 if 'There are no relocations' not in rel: raise SystemExit('BootShim contains relocations')
 nm=subprocess.check_output([a64prefix+'nm','-n',str(obj)],text=True)
 payload=int(next(x.split()[0] for x in nm.splitlines() if x.endswith(' _Payload')),16)
 if payload!=len(data) or struct.unpack_from('<QQ',data,0x48)!=(0x41080000,0x300000):
  raise SystemExit('BootShim payload or FD layout mismatch')
 combined=fd.parent/'SILICIUM_UEFI.fd.shimmed'
 with combined.open('wb') as dest, fd.open('rb') as src:
  dest.write(data); shutil.copyfileobj(src,dest)
 names=('gcc','ld','objcopy','readelf','nm')
 result['aarch64_tools']={n:sha(Path(shutil.which(a64prefix+n) or a64prefix+n)) for n in names}
 result['build_defines'].update(A04BR3_EXPERIMENTAL_THREE_CPU='1',A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT='1')
 result['shim_defines']={'UEFI_BASE':'0x41080000','UEFI_SIZE':'0x300000','A04BR3_A64_CPU_TEST':1,
  'A04BR3_A32_SECONDARY_TEST':1,'A04BR3_PARK_TEST':1,'A04BR3_SMP_TEST':1,'A04BR3_WDT_HANDOFF_TEST':0,
  'A04BR3_FOUR_CPU_TEST':1 if three == '4' else 0,'A04BR3_A64_WDT_TEST':1,'A04BR3_A64_WDT_SECONDS':120 if three == '2' else 420,'A04BR3_KERNEL_MARKER':1 if three in ('3','4') else 0,
  'A04BR3_FB_MONITOR':1 if three in ('3','4') else 0,'A04BR3_OFFLINE_WDT_UNBOUNDED':0 if three in ('2','3','4') else 1,'A04BR3_WORKER_NO_UART_RESET':0 if three in ('2','3','4') else 1,'A04BR3_WORKER_HEARTBEAT':1}
 result['shim_defines']['A04BR3_FOUR_CPU_L1_CLEAN']=int(cpu1_l1)
 result['shim_defines']['A04BR3_EARLY_FDT_GUARD']=1 if three in ('2','3','4') else 0
 result.update(shim_sha256=sha(shim),shim_bytes=len(data),shimmed_fd_sha256=sha(combined),shimmed_fd_bytes=combined.stat().st_size)
 result['limitations']=[('experimental four CPU; requires verified boot-start watchdog' if three == '4' else 'experimental three CPU; no runtime validation'),'worker heartbeat is not proof of watchdog operation',
  'other reset sources not excluded','DISM forbidden','RP attribute unsupported',
  'MMU runtime, cache and NVS unverified','libgcc ABI warnings unreviewed']
 if overlay:
  result['libgcc_overlay']={'libgcc_sha256':sha(Path(overlay)/'libgcc.a'),
                            'manifest_sha256':sha(Path(overlay)/'manifest.json')}
  result['limitations'][-1]='experimental libgcc overlay; firmware link and target ABI not verified'
 if three in ('2','3','4'):
  result['build_defines']['A04BR3_BOOT_DIAGNOSTIC']='1' if three == '2' else '0'
  result['build_defines']['A04BR3_BOUNDED_RECOVERY']='1'
  result['build_defines']['A04BR3_FOUR_CPU_TEST']='1' if three == '4' else '0'
  result['build_defines']['A04BR3_CACHE_TEST']='1' if three in ('3','4') else '0'
  result['shim_c_flags']=['-O2','-ffreestanding','-fno-stack-protector','-fno-builtin','-fno-pic','-fno-pie',
                          '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only','-mstrict-align','-DA04_FDT_TRACE=1']
  result['limitations'].append('bounded watchdog; RTC fastboot flag is modified')
  result['limitations'].append('UEFI ReadyToBoot and ExitBootServices block OS launch' if three == '2' else ('four CPU handoff stops worker; Windows watchdog must take over; unvalidated' if three == '4' else 'OS entry enabled; automatic watchdog reset after bounded feed; hardware behavior unverified'))
if battery == '1': result['limitations'].append('LC709203F ACPI and I2C2 handoff have not been validated on this firmware')
if thermal == '1': result['limitations'].append('thermal observation is not firmware or EL3 ownership release')
if bq == '1': result['limitations'].append('BQHR stopped record transport untested; BQ communication remains disabled')
(fd.parent/'diagnostic-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
PY
