#!/usr/bin/env python3
"""Static integration checks; these do not prove firmware execution."""
from pathlib import Path
p = Path(__file__).resolve().parents[1]/'WindowsArm32/PrePi'
source = (p/'Pi.c').read_text()
inf = (p/'PrePi.inf').read_text()
entry = source.split('CEntryPoint(',1)[1].split('SecondaryCEntryPoint(',1)[0]
assert entry.count('PlatformInitialize();') == 1
assert 'A04BR3WatchdogArm' not in entry
assert entry.index('PlatformInitialize();') < entry.index('PrePiMain(')
assert '#include <Library/PlatformSecLib.h>' in source
assert 'PlatformSecLib' in inf and 'PlatformPrePiLib' not in inf
assert 'a04br3Pkg/a04br3.dec' in inf
print('PrePi SEC hook ownership and package references checked; no firmware build')
