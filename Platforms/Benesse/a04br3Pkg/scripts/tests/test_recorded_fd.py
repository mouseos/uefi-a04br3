#!/usr/bin/env python3
import importlib.util,os,subprocess,tempfile
from pathlib import Path
script=Path(__file__).resolve().parents[1]/'build-recorded-fd.py';spec=importlib.util.spec_from_file_location('recorded_fd',script);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as td:
 root=Path(td);base=root/'base';base.mkdir();patches=root/'Resources/MuPatches';patches.mkdir(parents=True)
 def git(*args):return subprocess.check_output(['git','-C',str(base),*args],text=True).strip()
 git('init','-q');(base/'a').write_text('old\n');git('add','a');git('commit','-qm','fixture')
 (base/'a').write_text('new\n');(patches/'x.patch').write_text(git('diff')+'\n')
 m.ROOT=root;m.PATCHES=('x.patch',)
 assert m.patched_tree(base,True)==m.patched_tree(base,False)
 (base/'a').write_text('unexpected\n');assert m.patched_tree(base,True)!=m.patched_tree(base,False)
 assert git('show','HEAD:a')=='old' and git('diff','--cached')==''
print('PASS patch tree verification and isolated index')
