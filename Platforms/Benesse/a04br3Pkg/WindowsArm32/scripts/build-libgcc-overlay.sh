#!/usr/bin/env bash
# Experimental libgcc overlay; compare every allocated section to the original.
set -euo pipefail
if [[ $# != 4 ]]; then
  echo 'usage: build-libgcc-overlay.sh GCC_SOURCE_TARBALL ARM_GCC_PREFIX QEMU_ARM OUTPUT_DIR' >&2
  exit 2
fi
archive=$(realpath "$1"); prefix=$2; qemu=$(realpath "$3"); output=$4
[[ ! -e $output ]] || { echo 'output exists' >&2; exit 2; }
expected=2db06abe865ce68eced25e449aecb45cdb6c151c3551c739620cfc3730214c91
[[ $(sha256sum "$archive" | cut -d' ' -f1) == "$expected" ]] || { echo 'unexpected GCC source archive' >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
work=$(mktemp -d "${TMPDIR:-/tmp}/a04-libgcc-overlay.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT
base=gcc-arm-none-eabi-13.2.rel1
tar -xjf "$archive" -C "$work" "$base/libgcc/libgcc2.c" "$base/libgcc/libgcc2.h" \
  "$base/libgcc/config/arm/bpabi-lib.h" "$base/include/longlong.h"
mkdir "$work/headers" "$work/old" "$work/new"
printf '#include <stddef.h>\n' > "$work/headers/tsystem.h"
printf '#include "config/arm/bpabi-lib.h"\n' > "$work/headers/libgcc_tm.h"
touch "$work/headers/tconfig.h" "$work/headers/coretypes.h" "$work/headers/tm.h"
gcc="${prefix}gcc"; ar="${prefix}ar"
original=$("$gcc" -march=armv7-a -mthumb -mfloat-abi=soft -print-libgcc-file-name)
[[ -f $original ]] || exit 2
functions=(divdi3 moddi3 muldi3 ucmpdi2 udivdi3 udivmoddi4 umoddi3)
for func in "${functions[@]}"; do
  ( cd "$work/old" && "$ar" x "$original" "_${func}.o" )
  flags=(-O2 -march=armv7-a -mthumb -mfloat-abi=soft -fshort-wchar -fno-short-enums -fno-builtin)
  if "${prefix}readelf" -SW "$work/old/_${func}.o" | grep -q ' .ARM.exidx '; then flags+=(-fexceptions); fi
  "$gcc" -c "${flags[@]}" -DMIN_UNITS_PER_WORD=4 -D"L_${func}" \
    -I"$work/headers" -I"$work/$base/libgcc" -I"$work/$base/include" \
    "$work/$base/libgcc/libgcc2.c" -o "$work/new/_${func}.o"
done
python3 - "$work" <<'PY'
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection
import sys
work=Path(sys.argv[1])
def inspect(path):
 with path.open('rb') as f:
  elf=ELFFile(f)
  if elf['e_machine'] != 'EM_ARM' or not elf.little_endian: raise RuntimeError('wrong ELF target')
  sections={s.name:(s['sh_type'],s['sh_flags'],s['sh_addralign'],s.data())
            for s in elf.iter_sections() if s['sh_flags'] & 2}
  syms=next(s for s in elf.iter_sections() if isinstance(s,SymbolTableSection))
  globals_=sorted((s.name,s['st_info']['bind'],s['st_info']['type'],s['st_value'],s['st_size'],s['st_shndx'])
                  for s in syms.iter_symbols() if s['st_info']['bind'] in ('STB_GLOBAL','STB_WEAK'))
  reloc=[]
  for s in elf.iter_sections():
   if isinstance(s,RelocationSection) and elf.get_section(s['sh_info'])['sh_flags'] & 2:
    table=elf.get_section(s['sh_link'])
    for r in s.iter_relocations():
     sym=table.get_symbol(r['r_info_sym'])
     name=sym.name or (elf.get_section(sym['st_shndx']).name if isinstance(sym['st_shndx'],int) and sym['st_shndx'] else '')
     reloc.append((elf.get_section(s['sh_info']).name,r['r_offset'],r['r_info_type'],name,r.entry.get('r_addend')))
  return sections,globals_,sorted(reloc)
for old in sorted((work/'old').glob('*.o')):
 if inspect(old)!=inspect(work/'new'/old.name): raise SystemExit(f'allocated sections, globals or relocations differ: {old.name}')
 print(old.name, 'allocated sections, globals and relocations identical')
PY
mkdir -p "$output"
cp "$original" "$output/libgcc.a"
"$ar" rD "$output/libgcc.a" "$work/new/"*.o
src="$root/Platforms/Benesse/a04br3Pkg/Tests/check_arm_libgcc_runtime.c"
"$gcc" -march=armv7-a -mthumb -mfloat-abi=soft -fshort-wchar -fno-short-enums -fno-builtin \
  -fomit-frame-pointer -nostdlib -static -Wl,-e,_start -Wl,--no-undefined "$src" \
  -L"$output" -lgcc -o "$work/runtime.elf" 2>"$output/test-link.log"
[[ ! -s $output/test-link.log ]] || { cat "$output/test-link.log" >&2; exit 1; }
"$qemu" "$work/runtime.elf"
python3 - "$archive" "$original" "$output" "$gcc" "$qemu" <<'PY'
import hashlib,json,os,subprocess,sys
archive,original,out,gcc,qemu=sys.argv[1:]
def sha(p):
 with open(p,'rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
data={'status':'experimental-not-integrated','gcc_source_archive_sha256':sha(archive),
      'original_libgcc_sha256':sha(original),'overlay_libgcc_sha256':sha(out+'/libgcc.a'),
      'gcc_sha256':sha(gcc),'qemu_sha256':sha(qemu),'qemu_test_vectors':4096,
      'validation':'seven rebuilt objects match original allocated sections, globals and relocations',
      'limitations':['generated GCC configuration headers are minimal, not official build outputs',
                     'firmware link and target execution not verified']}
open(out+'/manifest.json','w').write(json.dumps(data,indent=2)+'\n')
print(json.dumps(data,indent=2))
PY
