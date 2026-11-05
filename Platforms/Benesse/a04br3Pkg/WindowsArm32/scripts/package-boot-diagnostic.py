#!/usr/bin/env python3
"""Package a bounded non-DISM diagnostic using the A04BR3 stock boot layout.

Based on A04BR3Pkg/package_boot_image.py in mouseos/edk2-mtk b8ca32e.
The supplied stock image and DTB are inputs, not claimed as canonical UEFI.
SPDX-License-Identifier: BSD-2-Clause-Patent
"""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def main():
    p = argparse.ArgumentParser()
    p.add_argument('firmware', type=Path)
    p.add_argument('firmware_manifest', type=Path)
    p.add_argument('stock_boot', type=Path)
    p.add_argument('dtb', type=Path)
    p.add_argument('output_dir', type=Path)
    a = p.parse_args()
    fw, mf, stock, dtb = (x.resolve(strict=True) for x in (a.firmware, a.firmware_manifest, a.stock_boot, a.dtb))
    out = a.output_dir.resolve()
    if out.exists(): raise SystemExit('output already exists')
    m = json.loads(mf.read_text())
    s = m.get('shim_defines', {})
    if (m.get('status') != 'diagnostic-only-not-for-device' or m.get('shimmed_fd_sha256') != sha(fw) or
        m.get('build_defines', {}).get('A04BR3_BOOT_DIAGNOSTIC') not in ('0', '1') or
        m.get('build_defines', {}).get('A04BR3_BOUNDED_RECOVERY') != '1' or
        s.get('A04BR3_A64_WDT_SECONDS') != (120 if m['build_defines']['A04BR3_BOOT_DIAGNOSTIC'] == '1' else 420) or s.get('A04BR3_OFFLINE_WDT_UNBOUNDED') != 0 or
        s.get('A04BR3_WORKER_HEARTBEAT') != 1 or s.get('A04BR3_WORKER_NO_UART_RESET') != 0 or
        s.get('A04BR3_EARLY_FDT_GUARD') != 1):
        raise SystemExit('firmware manifest is not the bounded diagnostic')
    blob = dtb.read_bytes()
    if len(blob) < 40 or blob[:4] != b'\xd0\x0d\xfe\xed' or struct.unpack_from('>I',blob,4)[0] != len(blob):
        raise SystemExit('invalid DTB')
    abootimg, gzip = shutil.which('abootimg'), shutil.which('gzip')
    if not abootimg or not gzip: raise SystemExit('abootimg and gzip are required')
    with tempfile.TemporaryDirectory(prefix='a04-diagnostic-boot-') as tmp:
        t = Path(tmp)
        def extract(image, stem):
            paths=[t/(stem+n) for n in ('.cfg','.kernel','.ramdisk')]
            subprocess.run([abootimg,'-x',str(image),*(str(x) for x in paths)], check=True, stdout=subprocess.DEVNULL)
            return paths
        image_header=stock.read_bytes()[:48]
        if image_header[:8] != b'ANDROID!': raise SystemExit('invalid Android boot header')
        _, kernel_addr, ramdisk_size, ramdisk_addr, second_size, _, tags_addr, page_size = struct.unpack_from('<8I',image_header,8)
        if (kernel_addr,ramdisk_addr,tags_addr,page_size,second_size)!=(0x40080000,0x44000000,0x4e000000,2048,0):
            raise SystemExit('unexpected stock load addresses or Android boot layout')
        if ramdisk_addr + ramdisk_size > 0x44400000 or tags_addr + len(blob) > 0x4e240000:
            raise SystemExit('stock ramdisk or DTB overlaps fixed reservations')
        if kernel_addr + len(fw.read_bytes()) > 0x41080000:
            raise SystemExit('expanded kernel overlaps FD destination')
        _, kernel, ramdisk = extract(stock, 'stock')
        old=kernel.read_bytes()
        if len(old)<512 or old[8:15]!=b'KERNEL\0' or struct.unpack_from('<I',old,4)[0]!=len(old)-512:
            raise SystemExit('stock MTK kernel header mismatch')
        compressed=subprocess.check_output([gzip,'-n','-9','-c',str(fw)])
        payload=compressed + b'\0' * (-len(compressed)%512) + blob
        header=bytearray(old[:512]); struct.pack_into('<I',header,4,len(payload))
        replacement=t/'replacement'; replacement.write_bytes(header+payload)
        candidate=t/'boot.img'; shutil.copyfile(stock,candidate)
        subprocess.run([abootimg,'-u',str(candidate),'-k',str(replacement)],check=True,stdout=subprocess.DEVNULL)
        _, verify_kernel, verify_ramdisk=extract(candidate,'verify')
        if verify_kernel.read_bytes()!=replacement.read_bytes() or verify_ramdisk.read_bytes()!=ramdisk.read_bytes():
            raise SystemExit('boot image verification failed')
        if subprocess.check_output([gzip,'-dc'],input=compressed)!=fw.read_bytes():
            raise SystemExit('firmware gzip roundtrip failed')
        out.mkdir(parents=True)
        shutil.copyfile(candidate,out/'boot.img')
        result={'status':'bounded-os-entry-unvalidated' if m['build_defines']['A04BR3_BOOT_DIAGNOSTIC'] == '0' else 'bounded-diagnostic-not-validated-on-device','uefi_source_commit':m['source_commit'],
                'firmware_manifest_sha256':sha(mf),'firmware_sha256':sha(fw),'stock_boot_sha256':sha(stock),
                'dtb_sha256':sha(dtb),'stock_ramdisk_sha256':sha(ramdisk),'boot_img_sha256':sha(out/'boot.img'),
                'packager_sha256':sha(Path(__file__)),'abootimg_sha256':sha(Path(abootimg)),
                'gzip_sha256':sha(Path(gzip)),
                'android_load_addresses':{'kernel':hex(kernel_addr),'ramdisk':hex(ramdisk_addr),'tags':hex(tags_addr)},
                'limitations':['non-DISM diagnostic; no device validation','RTC fastboot flag is modified',
                               'watchdog automatically resets after the finite feed deadline',
                               'DTB and stock boot inputs require provenance review',
                               'LK runtime FDT growth, initrd placement and dynamic reservations are unverified']}
        (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
if __name__ == '__main__': main()
