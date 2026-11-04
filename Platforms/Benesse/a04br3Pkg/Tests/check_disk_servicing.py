#!/usr/bin/env python3
"""Test actual recorded-build option validation; does not emulate firmware."""
import importlib.util
from pathlib import Path
p = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_recorded_fd', p/'scripts/build-recorded-fd.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
for mode, cycle, cpu, accepted in [('0','1','1',True), ('0','0','0',True),
                                  ('1','0','0',True), ('1','1','0',False),
                                  ('1','0','1',False), ('2','0','0',False)]:
    try:
        m.validate_disk_servicing(mode, cycle, cpu)
        got = True
    except RuntimeError:
        got = False
    assert got == accepted, (mode, cycle, cpu)
print('disk-servicing build option exclusion passed; hardware unverified')
