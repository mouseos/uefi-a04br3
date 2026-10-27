#!/usr/bin/env python3
"""Exercise read-only stage snapshot with exact MT8167 register addresses."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];h=(root/'Include/A04BR3CpuStage.h').read_text();sec=(root/'Library/A04BR3PlatformSecBoardLib/A04BR3PlatformSecBoardLib.c').read_text();dxe=(root/'Drivers/A04BR3TestCycleDxe/A04BR3TestCycleDxe.c').read_text();dsc=(root/'a04br3.dsc').read_text()
assert 'MmioWrite' not in h and 'A04BR3_CPU_STAGE_DIAG = 0' in dsc and 'A04BR3_CPU_STAGE_DIAG requires A04BR3_TEST_CYCLE=1' in dsc
for source,stage in ((sec,'SEC'),(dxe,'EBS')):
 assert re.search(r'#if A04BR3_CPU_STAGE_DIAG == 1 && !defined\(MDEPKG_NDEBUG\)\s*if \(Flag\) \{\s*CONST CHAR8 \*Stage = "'+stage+r'";',source)
 assert source.count('A04BR3CpuStageRead (MmioRead32)')==1
 assert source.index('Flag = A04BR3SetFastbootFlag ();') < source.index('A04BR3CpuStageRead (MmioRead32)')
code='''#include <stdint.h>\n#include <assert.h>\ntypedef uint32_t UINT32;typedef uintptr_t UINTN;\n#define STATIC static\n#define EFIAPI\n'''+h+r'''
static UINTN seen[8];static unsigned count;
static UINT32 fake(UINTN a){assert(count<8);seen[count++]=a;return (UINT32)a ^ 0x12345678U;}
int main(void){A04BR3_CPU_STAGE s=A04BR3CpuStageRead(fake);const UINTN expect[]={0x10001000U,0x10001014U,0x10018100U,0x10018104U,0x10018110U,0x10000010U,0x10000028U,0x1000002cU};assert(count==8);for(unsigned i=0;i<8;i++)assert(seen[i]==expect[i]);assert(s.InfraMux==(expect[0]^0x12345678U));assert(s.ArmDcm==(expect[1]^0x12345678U));assert(s.ArmPllCon1==(expect[3]^0x12345678U));assert(s.BusDcm1==(expect[7]^0x12345678U));}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS default-off SEC/EBS gates and exact read-only register order')
