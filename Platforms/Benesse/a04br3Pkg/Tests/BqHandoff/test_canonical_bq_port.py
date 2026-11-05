#!/usr/bin/env python3
"""Check staged provenance and that the FDT platform cannot enable BQHR."""
from pathlib import Path
import hashlib
import re

pkg = Path(__file__).resolve().parents[2]
expected = {
    'Drivers/I2c2HandoffDxe/I2c2HandoffDxe.c': 'a00be56d480d0c82af746e5af03542e64d23e246f988a9d97fdfd59ac2cb0f0e',
    'Drivers/I2c2HandoffDxe/BqHandoffAml.h': 'e1ab3a3cd479ea3159df0e3ed7aa9152db46681b3f91f4ca531bd176e2786823',
    'Library/Bq24160HandoffLib/charger.c': '594fa4889c7b36ffc790c28e2302c717740e394ae2cc3ddd0c31edde893b8e1c',
    'Library/Bq24160HandoffLib/BqFirmwareOwner.c': '34df7b026ac56b116c94b3d945851bd9faa7a63a73f284bd3493510d34607758',
    'Library/Bq24160HandoffLib/BqPio.c': '278a87aa9c287b1cceca6415253b3d3d3fe141d20c56c75713ae5b9c15f63848',
}
for name, digest in expected.items():
    assert hashlib.sha256((pkg/name).read_bytes()).hexdigest() == digest, name
dsc=(pkg/'a04br3.dsc').read_text()
fdf=(pkg/'a04br3.fdf').read_text()
dxe=(pkg/'Include/DXE.inc').read_text()
assert re.search(r'!ifndef A04BR3_WINDOWS_BQHR\s+DEFINE A04BR3_WINDOWS_BQHR\s*=\s*0\s+!endif',dsc)
assert re.search(r'!if \$\(A04BR3_WINDOWS_BQHR\) == 1\s+!error .*ARM32/ACPI',dsc)
assert 'SUPPORTED_ARCHITECTURES        = AARCH64' in dsc
assert 'I2c2HandoffDxe.inf' not in dsc+fdf+dxe
assert re.search(r'!if \$\(A04BR3_CHARGER_OWNER\) == 1\s+.*A04BR3ChargerDxe.inf',dsc,re.S)
assert re.search(r'BqAdmissionVerified\(UINT64 \*FirstDeadlineNs\)\s*\{ \(VOID\)FirstDeadlineNs; return FALSE; \}',(pkg/'Drivers/I2c2HandoffDxe/I2c2HandoffDxe.c').read_text())
print('Canonical BQHR provenance and disabled guard: PASS (static check)')
