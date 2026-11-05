#!/usr/bin/env python3
"""Rebuild a provenance-checked three-CPU shim with a longer finite deadline.

The original FD is reused unchanged. The original shim must reproduce exactly
before another duration is built. This does not establish target behavior.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--fd', type=Path, required=True)
p.add_argument('--manifest', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--seconds', type=int, required=True)
p.add_argument('--prefix', default='aarch64-linux-gnu-')
a = p.parse_args()
if not 420 < a.seconds <= 1800 or a.output.exists():
    raise SystemExit('seconds must be 421..1800 and output must not exist')
m = json.loads(a.manifest.read_text())
s = m.get('shim_defines', {})
if (m.get('status') != 'diagnostic-only-not-for-device' or sha(a.fd) != m.get('fd_sha256') or
    s.get('A04BR3_FOUR_CPU_TEST') != 0 or s.get('A04BR3_A64_WDT_SECONDS') != 420 or
    s.get('A04BR3_OFFLINE_WDT_UNBOUNDED') != 0 or s.get('A04BR3_WORKER_NO_UART_RESET') != 0 or
    s.get('A04BR3_EARLY_FDT_GUARD') != 1 or m.get('build_defines', {}).get('A04BR3_BOOT_DIAGNOSTIC') != '0'):
    raise SystemExit('source FD is not the expected bounded three-CPU build')
root = Path(subprocess.check_output(['git', '-C', str(Path(__file__).parent), 'rev-parse', '--show-toplevel'], text=True).strip())
shimdir = root / 'Platforms/Benesse/a04br3Pkg/WindowsArm32/BootShim'
relative = str(shimdir.relative_to(root))
if subprocess.run(['git', '-C', str(root), 'diff', '--quiet', m['source_commit'], 'HEAD', '--', relative]).returncode:
    raise SystemExit('shim sources differ from the FD source commit')
if subprocess.run(['git', '-C', str(root), 'diff', '--quiet', '--', relative]).returncode:
    raise SystemExit('shim sources have uncommitted changes')
tools = {}
for name in ('gcc', 'ld', 'objcopy'):
    found = shutil.which(a.prefix + name)
    if not found or sha(Path(found)) != m['aarch64_tools'][name]:
        raise SystemExit('AArch64 tool mismatch: ' + name)
    tools[name] = found
defines = {str(k): str(v) for k, v in s.items()}
with tempfile.TemporaryDirectory(prefix='a04-long-worker-') as directory:
    tmp = Path(directory)
    linker = tmp / 'shim.ld'
    linker.write_text('SECTIONS { . = 0; .text : { *(.text*) *(.rodata*) . = ALIGN(16); *(.payload*) }\n /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) } }\n')
    for name in ('FdtGuard', 'EarlyRecovery'):
        run([tools['gcc'], '-c', *m['shim_c_flags'], str(shimdir / (name + '.c')), '-o', str(tmp / (name + '.o'))])
    def build(seconds, suffix):
        flags = {**defines, 'A04BR3_A64_WDT_SECONDS': str(seconds)}
        obj, linked, binary = (tmp / (suffix + x) for x in ('.o', '.linked', '.bin'))
        run([tools['gcc'], '-c', str(shimdir / 'BootShim.S'), '-o', str(obj), *['-D' + k + '=' + v for k, v in flags.items()]])
        run([tools['ld'], '-T', str(linker), '-o', str(linked), str(obj), str(tmp / 'FdtGuard.o'), str(tmp / 'EarlyRecovery.o')])
        run([tools['objcopy'], '-O', 'binary', str(linked), str(binary)])
        return binary
    original = build(420, 'original')
    if sha(original) != m.get('shim_sha256') or hashlib.sha256(original.read_bytes() + a.fd.read_bytes()).hexdigest() != m.get('shimmed_fd_sha256'):
        raise SystemExit('original shim did not reproduce; refusing derived build')
    rebuilt = build(a.seconds, 'extended')
    a.output.mkdir(parents=True)
    combined = a.output / 'SILICIUM_UEFI.fd.shimmed'
    combined.write_bytes(rebuilt.read_bytes() + a.fd.read_bytes())
    m['shim_defines']['A04BR3_A64_WDT_SECONDS'] = a.seconds
    m['shim_sha256'] = sha(rebuilt)
    m['shim_bytes'] = rebuilt.stat().st_size
    m['shimmed_fd_sha256'] = sha(combined)
    m['shimmed_fd_bytes'] = combined.stat().st_size
    m['derived_from_manifest_sha256'] = sha(a.manifest)
    m['derived_shim_source_commit'] = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    m['limitations'].append(f'three-CPU watchdog worker has a {a.seconds}-second finite feed limit; not device validated')
    (a.output / 'diagnostic-manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('Prepared bounded shim', combined, sha(combined))
