#!/usr/bin/env python3
"""Operator-run bounded boot diagnostic; never changes ESP or resets on timeout."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('serial');p.add_argument('candidate',type=Path);p.add_argument('manifest',type=Path);p.add_argument('firmware_manifest',type=Path)
    p.add_argument('restore_boot',type=Path);p.add_argument('restore_sha256')
    p.add_argument('uart_log',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--wait-seconds',type=int,default=240)
    a=p.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9._-]+',a.serial) or not re.fullmatch(r'[0-9a-f]{64}',a.restore_sha256):
        raise SystemExit('invalid serial or restore hash')
    if not 1<=a.wait_seconds<=1800: raise SystemExit('invalid wait')
    for x in (a.candidate,a.manifest,a.firmware_manifest,a.restore_boot,a.uart_log):
        if not x.is_file(): raise SystemExit(f'missing file: {x}')
    m=json.loads(a.manifest.read_text())
    if m.get('status')!='bounded-diagnostic-not-validated-on-device' or m.get('boot_img_sha256')!=sha(a.candidate):
        raise SystemExit('candidate or manifest mismatch')
    firmware=json.loads(a.firmware_manifest.read_text())
    if (m.get('firmware_manifest_sha256')!=sha(a.firmware_manifest) or
        m.get('uefi_source_commit')!=firmware.get('source_commit') or
        firmware.get('build_defines',{}).get('A04BR3_BOOT_DIAGNOSTIC')!='1' or
        firmware.get('shim_defines',{}).get('A04BR3_EARLY_FDT_GUARD')!=1):
        raise SystemExit('bounded guard firmware provenance mismatch')
    for image in (a.candidate,a.restore_boot):
        with image.open('rb') as f:
            if f.read(8)!=b'ANDROID!': raise SystemExit('invalid boot image header')
    if sha(a.restore_boot)!=a.restore_sha256: raise SystemExit('restore image hash mismatch')
    if a.output.exists(): raise SystemExit('output already exists')
    a.output.mkdir(parents=True)
    lockpath=Path(os.environ.get('XDG_RUNTIME_DIR','/tmp'))/('a04br3-boot-diagnostic-'+a.serial+'.lock')
    with lockpath.open('a') as lock:
        try: fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: raise SystemExit('another boot diagnostic owns the host lock')
        log=(a.output/'commands.log').open('a',buffering=1)
        def call(args,timeout=None):
            log.write('CALL '+ ' '.join(args)+'\n')
            try: r=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout,text=True)
            except (subprocess.TimeoutExpired,OSError) as e:
                log.write('ERROR '+str(e)+'\n');return None
            log.write(r.stdout+'\nEXIT '+str(r.returncode)+'\n');return r
        def present():
            r=call(['fastboot','devices'],timeout=10)
            if r is None or r.returncode:return None
            return any(line.split()[:2]==[a.serial,'fastboot'] for line in r.stdout.splitlines())
        def product():
            r=call(['fastboot','-s',a.serial,'getvar','product'],timeout=10)
            return r is not None and r.returncode==0 and re.search(r'(?m)^\s*(?:\(bootloader\)\s*)?product:\s*A04BR3\s*$',r.stdout) is not None
        if present() is not True or not product(): raise SystemExit('matching fastboot target not confirmed; no writes')
        start=a.uart_log.stat().st_size
        info={'candidate_sha256':sha(a.candidate),'restore_sha256':a.restore_sha256,'serial':a.serial,'uart_offset':start,
              'manifest_sha256':sha(a.manifest),'esp_action':'none','restore_status':'not attempted'}
        def record(status):
            info['state']=status
            try:
                with a.uart_log.open('rb') as u:
                    if os.fstat(u.fileno()).st_size < start: raise OSError('UART log truncated')
                    u.seek(start); (a.output/'uart-since-start.log').write_bytes(u.read())
            except OSError as e: info['uart_capture_error']=str(e)
            (a.output/'state.json').write_text(json.dumps(info,indent=2)+'\n');print(status,flush=True)
        record('candidate flash starting')
        # No timeout: interrupting a host write can leave boot content uncertain.
        r=call(['fastboot','-s',a.serial,'flash','boot',str(a.candidate)])
        if r is None or r.returncode: record('flash uncertain; no automatic reset or restore');return 3
        record('candidate flashed; reboot starting')
        r=call(['fastboot','-s',a.serial,'reboot'],timeout=20)
        if r is None or r.returncode: record('reboot uncertain; no automatic reset or restore');return 3
        deadline=time.monotonic()+a.wait_seconds; departed=False
        while time.monotonic()<deadline:
            state=present()
            if state is False: departed=True
            if departed and state is True and product():
                record('fastboot returned; restoring boot only')
                r=call(['fastboot','-s',a.serial,'flash','boot',str(a.restore_boot)])
                if r is None or r.returncode: record('restore uncertain; leave device in fastboot');return 4
                info['restore_status']='fastboot flash reported success; no readback'
                record('boot restore command succeeded; ESP unchanged');return 0
            time.sleep(2)
        record('fastboot not observed after departure; no reset or restore');return 5
if __name__=='__main__': raise SystemExit(main())
