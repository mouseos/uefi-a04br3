#!/usr/bin/env python3
"""Compare the board ARM32 MMU attribute ABI with the selected Mu header."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('mu_basecore', type=Path)
a = p.parse_args()
board = Path(__file__).resolve().parents[1]
headers = (board / 'Include/A04ArmLib.h', a.mu_basecore / 'MdePkg/Include/Library/ArmLib.h')

def enum(path, prefix):
    source = path.read_text()
    matches = re.findall(r'typedef\s+enum\s*\{([^{}]*)\}\s*ARM_MEMORY_REGION_ATTRIBUTES\s*;', source, re.S)
    if len(matches) != 1:
        raise SystemExit(f'expected exactly one attribute enum: {path}')
    body = re.sub(r'//[^\n]*|/\*.*?\*/', '', matches[0], flags=re.S)
    return re.sub(r'\bARM_MEMORY_REGION_ATTRIBUTE_', prefix, body)

names = ('UNCACHED_UNBUFFERED', 'WRITE_BACK', 'WRITE_BACK_NONSHAREABLE',
         'WRITE_BACK_RO', 'WRITE_BACK_XP', 'WRITE_THROUGH', 'DEVICE')
with tempfile.TemporaryDirectory(prefix='a04-mmu-abi-') as tmp:
    tmp = Path(tmp)
    assertions = '\n'.join(f'_Static_assert((int)board_{n} == (int)mu_{n}, "{n} ABI mismatch");' for n in names)
    code = 'enum {\n' + enum(headers[0], 'board_') + '\n};\nenum {\n' + enum(headers[1], 'mu_') + '\n};\n' + assertions + '\nint main(void) { return 0; }\n'
    (tmp/'check.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(tmp/'check.c'), '-o', str(tmp/'check')], check=True)
    subprocess.run([str(tmp/'check')], check=True)
print('Board ARM32 MMU and selected Mu memory attribute values match')
