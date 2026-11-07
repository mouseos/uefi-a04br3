#!/usr/bin/env python3
"""Regenerate only board camera AML; every intermediate stays under checkout."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=root/'Platforms/Benesse/a04br3Pkg/WindowsArm32/Drivers/CameraHandoffDxe'
out=root/'build/camera-handoff';out.mkdir(parents=True,exist_ok=True)
subprocess.run(['iasl','-p',str(out/'Camera'),str(source/'Camera.asl')],check=True)
data=(out/'Camera.aml').read_bytes()
assert data[:4]==b'SSDT' and sum(data)%256==0
for sentinel in [0x12345678,0x87654321]:assert data[36:].count(sentinel.to_bytes(4,'little'))==1
text='/* SPDX-License-Identifier: BSD-2-Clause-Patent; generated from Camera.asl */\nstatic const UINT8 mCameraAml[] = {\n'
for pos in range(0,len(data),12):text+=' '+','.join(f'0x{b:02x}' for b in data[pos:pos+12])+',\n'
(source/'CameraAml.h').write_text(text+'};\n')
