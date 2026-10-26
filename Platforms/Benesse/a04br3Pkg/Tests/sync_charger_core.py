#!/usr/bin/env python3
"""Explicit device-board Core -> firmware-board sync. Default is verify only."""
import argparse
from pathlib import Path
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--device-root', type=Path, required=True)
p.add_argument('--write', action='store_true')
p.add_argument('--loader-root', type=Path)
a = p.parse_args()
u = Path(__file__).resolve().parents[1]
for name in ('A04BR3ChargerCore.h', 'A04BR3ChargerBoardCore.h', 'A04BR3ChargerHandoff.h'):
    src, dst = a.device_root / 'package' / name, u / 'Include' / name
    if a.write:
        dst.write_bytes(src.read_bytes())
    if src.read_bytes() != dst.read_bytes():
        raise SystemExit(f'FAIL Core divergence: {name}; review and use --write explicitly')
print('PASS charger Core byte identity (device -> board firmware)')

if a.loader_root:
    src=a.loader_root/'DarwinBootPkg/Driver/BoardHandoff.h'
    dst=u/'Include/DarwinBootBoardHandoff.h'
    if a.write:dst.write_bytes(src.read_bytes())
    if src.read_bytes()!=dst.read_bytes():raise SystemExit('FAIL generic board-handoff protocol copies diverge')
    print('PASS generic loader/firmware protocol byte identity')
