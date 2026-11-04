#!/usr/bin/env python3
"""Audit supplied OTA, DTB and LK evidence without accessing the target."""
import argparse
import hashlib
import json
import struct
import subprocess
import zipfile
from pathlib import Path

def digest(data): return hashlib.sha256(data).hexdigest()
def read_ints(dtb, node, prop):
    text=subprocess.check_output(['fdtget','-t','x',str(dtb),node,prop],text=True)
    return [int(x,16) for x in text.split()]
def main():
    p=argparse.ArgumentParser()
    p.add_argument('ota',type=Path); p.add_argument('stock',type=Path)
    p.add_argument('dtb',type=Path); p.add_argument('edk2_mtk_repo',type=Path)
    a=p.parse_args(); stock=a.stock.read_bytes(); dtb=a.dtb.read_bytes()
    with zipfile.ZipFile(a.ota) as z:
        if z.read('boot.img') != stock: raise SystemExit('stock boot differs from OTA member')
        metadata=z.read('META-INF/com/android/metadata').decode('utf-8')
        if 'pre-device=TAB-A04-BR3\n' not in metadata or '02.00.000' not in metadata:
            raise SystemExit('OTA metadata mismatch')
        lk=z.read('lk.bin')
    ref='1982d888c765f5b12f6e0dce71f1ed8bc92ac50b'
    path='Platform/Benesse/A04BR3Pkg/Blob/mt8167-benesse-a04br3.dtb'
    tracked=subprocess.check_output(['git','-C',str(a.edk2_mtk_repo),'show',ref+':'+path])
    if tracked != dtb: raise SystemExit('DTB does not match pinned edk2-mtk blob')
    if len(dtb)<40 or dtb[:4]!=bytes.fromhex('d00dfeed'): raise SystemExit('invalid FDT header')
    total, _, _, reserve_off = struct.unpack_from('>4I',dtb,4)
    if total != len(dtb) or reserve_off < 40 or reserve_off % 8 or reserve_off >= total:
        raise SystemExit('invalid FDT reserve-map offset')
    reserve_map=[]
    while True:
        if reserve_off + 16 > total: raise SystemExit('unterminated FDT reserve map')
        address,size=struct.unpack_from('>QQ',dtb,reserve_off); reserve_off+=16
        if address == 0 and size == 0: break
        if not size or address+size > 1<<64: raise SystemExit('invalid FDT reserve-map entry')
        reserve_map.append((address,size))
    if stock[:8]!=b'ANDROID!': raise SystemExit('invalid boot header')
    kernel,ka,rd,ra,second,_,tags,page=struct.unpack_from('<8I',stock,8)
    if (ka,ra,tags,page,second)!=(0x40080000,0x44000000,0x4e000000,2048,0): raise SystemExit('unexpected boot layout')
    rd_off=page+((kernel+page-1)//page)*page
    raw=stock[rd_off:rd_off+rd]
    if len(raw)!=rd or raw[:4]!=bytes.fromhex('88168858') or raw[8:40].split(b'\0',1)[0]!=b'ROOTFS':
        raise SystemExit('invalid MTK ROOTFS header')
    size=struct.unpack_from('<I',raw,4)[0]
    if size != len(raw)-512 or ra+size>0x44400000: raise SystemExit('initrd outside reserved window')
    mem=read_ints(a.dtb,'/memory@00000000','reg')
    if mem != [0,0x40000000,0,0x20000000]: raise SystemExit('unexpected DTB low memory')
    nodes=subprocess.check_output(['fdtget','-l',str(a.dtb),'/reserved-memory'],text=True).splitlines()
    fixed={}; dynamic=[]
    for n in nodes:
        node='/reserved-memory/'+n
        result=subprocess.run(['fdtget','-t','x',str(a.dtb),node,'reg'],capture_output=True,text=True)
        if result.returncode: dynamic.append(node); continue
        values=[int(x,16) for x in result.stdout.split()]
        if len(values)%4: raise SystemExit('unsupported DTB reserved region cells')
        fixed[node]=[{'base':hex((values[i]<<32)|values[i+1]),'size':hex((values[i+2]<<32)|values[i+3])} for i in range(0,len(values),4)]
    critical=[(0x40080000,0x41080000),(0x41080000,0x41380000),(0x4e000000,0x52000000)]
    for address,size in reserve_map:
        if any(address<hi and lo<address+size for lo,hi in critical):
            raise SystemExit('FDT memory reservation block overlaps firmware')
    for node,regions in fixed.items():
        for x in regions:
            b=int(x['base'],16); e=b+int(x['size'],16)
            if any(b<hi and lo<e for lo,hi in critical): raise SystemExit('fixed DTB reservation conflicts: '+node)
    result={'status':'static-audit-only','ota_sha256':digest(a.ota.read_bytes()),'stock_boot_sha256':digest(stock),
            'ota_lk_sha256':digest(lk),'dtb_sha256':digest(dtb),'dtb_repository':'https://github.com/mouseos/edk2-mtk',
            'dtb_commit':ref,'dtb_git_blob':subprocess.check_output(['git','-C',str(a.edk2_mtk_repo),'rev-parse',ref+':'+path],text=True).strip(),
            'ota_metadata':metadata,'rootfs_header_bytes':512,'rootfs_payload_sha256':digest(raw[512:]),
            'predicted_initrd_start':hex(ra),'predicted_initrd_end':hex(ra+size),
            'uefi_initrd_reservation':['0x44000000','0x44400000'],
            'dtb_fixed_reservations':fixed,'dtb_dynamic_reservations':dynamic,
            'dtb_memreserve':[{'base':hex(b),'size':hex(s)} for b,s in reserve_map],
            'limitations':['OTA identity is not certificate or hardware LK verification',
                           'LK runtime FDT changes, dynamic reservations and active target LK not observed',
                           'runtime FDT pointer and actual initrd need early firmware validation']}
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
