#!/usr/bin/env python3
"""Exercise actual guard on generated FDTs; no target access."""
import ctypes
from pathlib import Path
import subprocess
import tempfile

board=Path(__file__).resolve().parents[1]
source=board/'WindowsArm32/BootShim/FdtGuard.c'
dts='''/dts-v1/;
/ { #address-cells = <2>; #size-cells = <2>;
  memory@00000000 { device_type = "memory"; reg = <0 0x40000000 0 0x20000000>; };
  chosen { linux,initrd-start = <0x44000000>; linux,initrd-end = <0x4420612d>; };
  reserved-memory { #address-cells = <2>; #size-cells = <2>; ranges;
    test@43000000 { reg = <0 0x43000000 0 0x30000>; };
  };
};'''
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp); so=root/'guard.so'; source_dts=root/'base.dts'; image=root/'base.dtb'
    source_dts.write_text(dts)
    subprocess.run(['cc','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(source),'-o',str(so)],check=True)
    subprocess.run(['dtc','-q','-I','dts','-O','dtb','-o',str(image),str(source_dts)],check=True)
    guard=ctypes.CDLL(str(so)).A04FdtGuard
    guard.argtypes=[ctypes.c_void_p,ctypes.c_size_t];guard.restype=ctypes.c_int
    def check(data,ok):
        buf=ctypes.create_string_buffer(bytes(data))
        result=guard(buf,len(data))
        if (result==0)!=ok: raise AssertionError((result,ok))
    baseline=image.read_bytes();check(baseline,True)
    # The FDT structure itself is 8-aligned, but individual 64-bit reg
    # values can be only 4-aligned. Exercise a deliberately offset buffer too.
    shifted=ctypes.create_string_buffer(len(baseline)+8)
    ctypes.memmove(ctypes.addressof(shifted)+4,baseline,len(baseline))
    assert guard(ctypes.addressof(shifted)+4,len(baseline))==0
    for capacity in range(40): check(baseline[:capacity],False)
    def case(node,prop,values):
        copy=root/'changed.dtb';copy.write_bytes(baseline)
        subprocess.run(['fdtput','-t','x',str(copy),node,prop,*values],check=True)
        return copy.read_bytes()
    check(case('/chosen','linux,initrd-end',['45000000']),False)
    check(case('/chosen','linux,initrd-start',['41080000']),False)
    check(case('/reserved-memory/test@43000000','reg',['0','41080000','0','1000']),False)
    check(case('/reserved-memory/test@43000000','reg',['0','51fff000','0','1000']),False)
    check(case('/memory@00000000','reg',['0','40000000','0','1000000']),False)
    dynamic=root/'dynamic.dts'; dynamic.write_text(dts.replace('reg = <0 0x43000000 0 0x30000>;','size = <0 0x30000>;'))
    subprocess.run(['dtc','-q','-I','dts','-O','dtb','-o',str(image),str(dynamic)],check=True)
    check(image.read_bytes(),False)
    # Only the board's exact unresolved dynamic pool is admitted; it does
    # not claim a physical address before the later allocator chooses one.
    dynamic.write_text(dts.replace('test@43000000 { reg = <0 0x43000000 0 0x30000>; };',
        'consys-reserve-memory { size = <0 0x200000>; alignment = <0 0x200000>; };'))
    subprocess.run(['dtc','-q','-I','dts','-O','dtb','-o',str(image),str(dynamic)],check=True)
    check(image.read_bytes(),True)
    dynamic.write_text(dynamic.read_text().replace('alignment = <0 0x200000>;', 'alignment = <0 0x100000>;'))
    subprocess.run(['dtc','-q','-I','dts','-O','dtb','-o',str(image),str(dynamic)],check=True)
    check(image.read_bytes(),False)
    reserved=root/'reserve.dts' ; reserved.write_text(dts.replace('/dts-v1/;', '/dts-v1/;\n/memreserve/ 0x41080000 0x1000;'))
    subprocess.run(['dtc','-q','-I','dts','-O','dtb','-o',str(image),str(reserved)],check=True)
    check(image.read_bytes(),False)
    bad=bytearray(baseline);bad[0]=0;check(bad,False)
print('FDT guard accepts fixture and rejects truncated, conflicting and unresolved reservations')
