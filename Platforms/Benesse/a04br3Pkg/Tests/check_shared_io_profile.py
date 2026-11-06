#!/usr/bin/env python3
"""Validate combined ACPI/PDO/DTS ownership and execute the actual SYSIRQ callback."""
import argparse,json,os,re,struct,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--drivers',type=Path,required=True);p.add_argument('--linux',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
pkg=Path(__file__).resolve().parents[1];board=pkg/'WindowsArm32';acpi=board/'Acpi'
contract=json.loads((acpi/'shared-io-contract.json').read_text())
a.out.mkdir(parents=True,exist_ok=True)
def git(ref,path):return subprocess.check_output(['git','-C',str(a.drivers),'show',f'{ref}:{path}'],text=True)
def check_source(ref):
 s=git(ref,'drivers/bus/mt8167-shared/wdf_parent_probe.c')
 assert 'descriptor.u.Memory.Length=c->base==0x11100000u?0x10000:0x1000' in s
 assert 'descriptor.u.Memory.MinimumAddress.QuadPart=0x11110000u' in s
 for vector in (104,141,152):
  assert f'descriptor.u.Interrupt.MinimumVector={vector}' in s
  assert f'descriptor.u.Interrupt.MaximumVector={vector}' in s
 assert s.count('descriptor.Flags=CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE')>=3
 for base in ('0x11140000','0x11141000','0x11170000u','0x11009000u','0x10002000u'):
  assert base in s
 assert 'descriptor.u.Memory.Length=0x9000' in s
 assert 'A04_AUDIO_CHILD_RESOURCES' in s and 'A04_SHARED_WIFI_IRQ_DIAGNOSTIC' in s
 return s
usb=check_source(contract['driver_sources']['usb_audio_parent']);wifi=check_source(contract['driver_sources']['wifi'])
assert 'A04_SHARED_USB_ONLY' in usb and 'A04_SHARED_FIRST_PLAYBACK' in usb
assert 'A04_SHARED_WIFI_RUNTIME' in wifi and 'wifiRtc' in wifi
# Both branches agree on resources; they still need one merged runtime parent.
linux_commit=subprocess.check_output(['git','-C',str(a.linux),'rev-parse','HEAD'],text=True).strip()
assert linux_commit==contract['linux_source']['commit']
for path in contract['linux_source']['paths']:
 relative=path.removeprefix('kernel-4.4/')
 assert subprocess.check_output(['git','-C',str(a.linux),'diff','HEAD','--',relative])==b''
dtsi=(a.linux/'arch/arm64/boot/dts/mediatek/mt8167.dtsi').read_text()
dts=(a.linux/'arch/arm64/boot/dts/mediatek/a04br3.dts').read_text()
sysirq=(a.linux/'drivers/irqchip/irq-mtk-sysirq.c').read_text();gic=(a.linux/'drivers/irqchip/irq-gic.c').read_text()
assert 'interrupt-parent = <&sysirq>' in dtsi
assert 'reg = <0 0x10200620 0 0x20>' in dtsi
assert 'offset = hwirq & 0x1f' in sysirq and 'reg_index = hwirq >> 5' in sysirq
assert 'type = IRQ_TYPE_LEVEL_HIGH' in sysirq and 'value |= (1 << offset)' in sysirq
assert 'gicirq >= 32 && type != IRQ_TYPE_LEVEL_HIGH' in gic
assert 'interrupts = < 5 IRQ_TYPE_LEVEL_LOW >' in dts # separate combo EINT, not GSI141
for name,spi in [('usb@11100000',72),('audio-controller@11140000',120),('sdio@11170000',109)]:
 node=dtsi[dtsi.index(name):];node=node[:node.index('};')]
 assert re.search(rf'interrupts\s*=\s*<GIC_SPI {spi} IRQ_TYPE_LEVEL_LOW>',node)
assert '<GIC_SPI 185 IRQ_TYPE_LEVEL_LOW>' in dtsi
# No overlap between synthetic child owners or the active ACPI resource owners.
inf=(acpi/'AcpiTablesSharedIo.inf').read_text();names=re.findall(r'^\s+(\S+\.asl)\s*$',inf,re.M)
assert set(names)=={'Dsdt.asl','GpuProvider.asl','DisplayProvider.asl','BatteryGauge.asl','ButtonArray.asl','SharedParentIo.asl'}
spans=[]
for name in names:
 for base,size in re.findall(r'Memory32Fixed\s*\(ReadWrite,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)',(acpi/name).read_text()):
  spans.append((int(base,16),int(size,16),name))
for child in contract['children']:
 spans.extend((base,size,child['id']) for base,size in child['memory'])
for i,(base,size,owner) in enumerate(spans):
 for other,amount,other_owner in spans[:i]:
  assert base+size<=other or other+amount<=base,(owner,other_owner)
# Execute AML: the parent owns exactly the existing 5 ranges and no child IRQs.
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
 tmp=Path(tmp);files=[]
 for name in names:
  target=tmp/Path(name).stem
  result=subprocess.run(['iasl','-p',str(target),str(acpi/name)],check=True,capture_output=True,text=True)
  (a.out/(Path(name).stem+'-iasl.log')).write_text(result.stdout+result.stderr)
  data=target.with_suffix('.aml').read_bytes();assert sum(data)%256==0
  files.append(str(target.with_suffix('.aml')))
 result=subprocess.run(['acpiexec','-b','evaluate _SB.SHR0._HID; evaluate _SB.SHR0._CRS; evaluate _SB.SHR0._CCA; evaluate _SB.BTT0._HID',*files],check=True,capture_output=True,text=True)
 (a.out/'namespace.log').write_text(result.stdout+result.stderr)
 assert 'AE_ALREADY_EXISTS' not in result.stdout+result.stderr and 'AE_NOT_FOUND' not in result.stdout+result.stderr
 assert '"BEN0001"' in result.stdout and '"MOU0002"' in result.stdout
 rows=re.findall(r'^\s*[0-9A-F]{4}:\s*((?:[0-9A-F]{2} )+)',result.stdout,re.M)
 blob=bytes.fromhex(' '.join(rows));assert len(blob)==62 and blob[-2:]==b'\x79\x00'
 ranges=[]
 for i in range(0,60,12):
  kind,length,access,base,size=struct.unpack_from('<BHBII',blob,i);assert (kind,length,access)==(0x86,9,1);ranges.append([base,size])
 assert ranges==contract['parent']['memory']
 assert '[Integer] = 0000000000000000' in result.stdout
 # Actual DXE callback with fake MMIO: exactly two masked writes, preserve all other IRQs.
 for name in ('Uefi.h','Guid/EventGroup.h','Library/DebugLib.h','Library/IoLib.h','Library/UefiBootServicesTableLib.h'):
  f=tmp/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('')
 pre='''#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
typedef uint32_t UINT32;typedef void VOID;typedef int BOOLEAN;typedef int EFI_STATUS;
typedef void *EFI_EVENT;typedef void *EFI_HANDLE;typedef void EFI_SYSTEM_TABLE;
#define STATIC static
#define EFIAPI
#define TRUE 1
#define BIT8 (1U<<8)
#define BIT13 (1U<<13)
#define BIT24 (1U<<24)
#define DEBUG_ERROR 1
#define EVT_NOTIFY_SIGNAL 1
#define TPL_CALLBACK 1
static unsigned writes,errors;static int reject;static UINT32 usb,media;
static UINT32 MmioRead32(uintptr_t a){if(a==0x10200628)return usb;assert(a==0x1020062c);return media;}
static VOID MmioWrite32(uintptr_t a,UINT32 v){++writes;if(reject)return;if(a==0x10200628)usb=v;else {assert(a==0x1020062c);media=v;}}
static void Debug(int level,const char *s,...){(void)level;if(strstr(s,"unavailable"))++errors;}
#define DEBUG(x) Debug x
static int gEfiEventBeforeExitBootServicesGuid;
static int EventEx(int a,int b,void(*c)(void*,void*),void*d,int*e,void**f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;}
static struct {int (*CreateEventEx)(int,int,void(*)(void*,void*),void*,int*,void**);} bs={EventEx},*gBS=&bs;
'''
 source=(board/'Drivers/SharedIoIrqDxe/SharedIoIrqDxe.c').resolve()
 code=pre+f'\n#include "{source}"\n'+'''int main(void){
 for(unsigned i=0;i<32;i++){
  UINT32 u=1U<<i,m=~u;usb=u;media=m;Attempted=0;writes=errors=0;
  assert(SharedIoIrqEntry(0,0)==0);SharedIoBeforeExit(0,0);
  assert(writes==2 && !errors && usb==(u|BIT8) && media==(m|BIT13|BIT24));
  SharedIoBeforeExit(0,0);assert(writes==2);
 }
 usb=media=0;Attempted=0;reject=1;errors=0;SharedIoBeforeExit(0,0);assert(errors==1);
 return 0;
}
'''
 (tmp/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(tmp),str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
 subprocess.run([str(tmp/'test')],check=True)
result={'status':'host-verified-not-device-validated','parent_memory':contract['parent']['memory'],'synthetic_children':contract['children'],'acpi_sources':names,'checks':['iasl/checksum/acpiexec','no legacy ACPI or MMIO duplicate','pinned driver PDO contracts','board DTS SPI and parent domain','Linux SYSIRQ inversion/GIC level-high','actual DXE masked writes and one-shot/error paths']}
(a.out/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
print('Shared IO profile: AML/PDO/DTS ownership and actual SYSIRQ callback passed; hardware untested')
