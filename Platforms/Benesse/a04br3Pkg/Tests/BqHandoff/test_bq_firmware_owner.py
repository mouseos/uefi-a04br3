#!/usr/bin/env python3
"""Compile and run the actual firmware owner against a fake bus; no hardware."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

pkg = Path(__file__).resolve().parents[2]
lib = pkg / 'Library/Bq24160HandoffLib'
inc = pkg / 'Include'
p = argparse.ArgumentParser()
p.add_argument('--windows-driver-root', type=Path)
args = p.parse_args()
core = lib / 'charger.c'
if args.windows_driver_root:
    win = args.windows_driver_root / 'drivers/power/bq24160'
    expected = (win / 'charger.c').read_text().replace(
        '#include "charger.h"', '#include <Library/A04BR3BqChargerCore.h>')
    if core.read_text() != expected or (inc / 'Library/A04BR3BqChargerCore.h').read_bytes() != (win / 'charger.h').read_bytes():
        raise SystemExit('Windows and firmware charger cores differ')
with tempfile.TemporaryDirectory(prefix='a04-bq-fw-') as temp:
    exe = str(Path(temp) / 'test')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(inc), str(core), str(lib / 'BqFirmwareOwner.c'),
                    str(pkg / 'Tests/BqHandoff/bq_firmware_owner_host.c'), '-o', exe], check=True)
    subprocess.run([exe], check=True)
    pio_exe = str(Path(temp) / 'pio')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(inc), str(lib / 'BqPio.c'),
                    str(pkg / 'Tests/BqHandoff/bq_firmware_pio_host.c'), '-o', pio_exe], check=True)
    subprocess.run([pio_exe], check=True)
    integrated = str(Path(temp) / 'integrated')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(inc), str(core), str(lib / 'BqFirmwareOwner.c'),
                    str(lib / 'BqPio.c'), str(pkg / 'Tests/BqHandoff/bq_firmware_integration_host.c'),
                    '-o', integrated], check=True)
    subprocess.run([integrated], check=True)
print('BQ firmware owner, stopped records and Windows handoff: PASS (host mock only)')
