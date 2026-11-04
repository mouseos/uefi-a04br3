#!/usr/bin/env python3
"""Use a fake fastboot executable; never accesses a device."""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
board=Path(__file__).resolve().parents[1]
runner=board/'WindowsArm32/scripts/run-boot-only-diagnostic.py'
with tempfile.TemporaryDirectory() as d:
    root=Path(d);bin=root/'bin';bin.mkdir();cmd=bin/'fastboot'
    cmd.write_text('''#!/usr/bin/env python3
import os,sys
from pathlib import Path
root=Path(os.environ['FAKE_ROOT']); args=sys.argv[1:]
with (root/'actions').open('a') as f: f.write(' '.join(args)+'\\n')
if args==['devices']:
 p=root/'count'; n=int(p.read_text()) if p.exists() else 0; p.write_text(str(n+1))
 if n==0 or (n>=2 and os.environ.get('FAKE_TIMEOUT')!='1'): print('SERIAL fastboot')
elif 'getvar' in args: print('(bootloader) product: A04BR3')
''');cmd.chmod(0o755)
    candidate=root/'candidate';candidate.write_bytes(b'ANDROID!candidate')
    restore=root/'restore';restore.write_bytes(b'ANDROID!restore')
    uart=root/'uart';uart.write_bytes(b'old')
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    fw=root/'fw.json';fw.write_text(json.dumps({'source_commit':'abc','build_defines':{'A04BR3_BOOT_DIAGNOSTIC':'1'},'shim_defines':{'A04BR3_EARLY_FDT_GUARD':1}}))
    manifest=root/'manifest.json';manifest.write_text(json.dumps({'status':'bounded-diagnostic-not-validated-on-device','boot_img_sha256':digest(candidate),'uefi_source_commit':'abc','firmware_manifest_sha256':digest(fw)}))
    env={**os.environ,'PATH':str(bin)+':'+os.environ['PATH'],'FAKE_ROOT':str(root),'XDG_RUNTIME_DIR':str(root)}
    args=['python3',str(runner),'SERIAL',str(candidate),str(manifest),str(fw),str(restore),digest(restore),str(uart)]
    r=subprocess.run(args+[str(root/'ok'),'--wait-seconds','10'],env=env,capture_output=True,text=True)
    assert r.returncode==0,(r.stdout,r.stderr)
    actions=(root/'actions').read_text(); assert actions.count('flash boot')==2 and 'flash esp' not in actions and 'reset' not in actions
    (root/'actions').unlink();(root/'count').unlink();env['FAKE_TIMEOUT']='1'
    r=subprocess.run(args+[str(root/'timeout'),'--wait-seconds','1'],env=env,capture_output=True,text=True)
    assert r.returncode==5,(r.stdout,r.stderr)
    actions=(root/'actions').read_text(); assert actions.count('flash boot')==1 and 'flash esp' not in actions and 'reset' not in actions
    assert 'no reset or restore' in (root/'timeout'/'state.json').read_text()
print('fake fastboot: boot-only restore on observed return; timeout performs no reset or restore')
