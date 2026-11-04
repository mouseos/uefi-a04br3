#!/usr/bin/env python3
"""Check the embedded 3-CPU SSDT against its ASL using the local iasl."""
from pathlib import Path
import re
import subprocess
import tempfile

board = Path(__file__).resolve().parents[1]
arm = board / "WindowsArm32"
header = arm / "Drivers/ThreeCpuDxe/CpuParkingSsdt.h"
embedded = bytes(int(n, 16) for n in re.findall(r"0x([0-9a-fA-F]{2})", header.read_text()))
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "cpu"
    subprocess.run(["iasl", "-p", str(output), str(arm / "Acpi/CpuParkingSsdt.asl")], check=True)
    compiled = output.with_suffix(".aml").read_bytes()
if embedded != compiled:
    raise SystemExit("SSDT bytes differ: check ASL and iasl version")
if len(embedded) != int.from_bytes(embedded[4:8], "little") or sum(embedded) % 256:
    raise SystemExit("invalid SSDT length or checksum")
print("SSDT ASL/header match; ACPI length and checksum valid")
