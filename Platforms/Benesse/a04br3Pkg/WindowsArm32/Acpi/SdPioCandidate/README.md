# MSDC1 PIO ACPI統合前の固定候補

RTCを優先し、実行sourceはd0a033c5のまま。これはSD ACPIの**非公開候補**で、
DSC/FDF/既存AcpiTablesSharedIo.infへは接続していない。既定ではSD nodeを増やさない。
D0の電源ownerが未合意のまま、単に_STA=0x0Fを公開しない。

## 固定sourceと一致した契約

Windows source f729a77d73dc84894e7e543ade2b20e89c5ab880
`drivers/storage/mt8167-sd/acpi/MT8167-SD.asl`と同一bytesを保持する。
Linux一次sourceは7c4782127d4ea11965be00ce1343056be9bfb769。

- MT8167.dtsiのmmc1=MSDC1、MMIO0x11130000/0x1000、SPI79→INTID111、level-low。
- a04br3.dtsのCD GPIO20/flags0/cd-invertedなし。native mmc binding経由のactive-high。
- pinctrl-mtk-common.cはMAX_GPIO_MODE_PER_REG=5/3bit/port stride16。
  GPIO20 mode=0x10005340 bits0..2、GPIO104=0x10005440 bits12..14、
  GPIO105..109=0x10005450 bits0..14。GPIO20 input=0x10005210 bit4、dir=0x10005010 bit4。
- _HID MOU0013、SDCD=raw CD、SDCF=GPIO/mux2のread-only確認。
- VMCH=VMMC、VMC=VQMMC。Linux supply phandleはD0保持のWindows所有契約そのものではない。
- PIO driverはINTEN=0、IRQ/DMAを接続しない。level-low記述をIRQ駆動の成功と扱わない。
  IRQを実使用する将来profileではSYSIRQ SPI79 inversionとHAL極性を別途一組で確定する。

## Nashへの未決条件

現ツールにNashの直接宛先がなく送信未実施。以下は調整要求であり合意済みではない。

1. VMCH/VMC 3.3Vとsource mux2/clock gateをD0期間保持する実owner/cleanup責務を固定する。
   SDCFはpin/muxの証拠だけで、rail状態・排他保持まで検証しない。
2. 既存eMMC/SDIO/WiFi/shared parentが同じrail/clockを変更しない根拠を照合する。
   UEFI ready状態をWindowsの全D0期間の保証へ流用しない。
3. MOU0013/MSDC1の重複ACPI/PDOがないこと、CD実極性、IRQをpoll専用とする契約を確定する。
4. 合意後に機種専用の既定OFF SD profileをDSC/FDFへ接続する。
   未検証RTCとSDを同時有効にせず、SD-onlyはRTC_READ=0/RTC_RUNTIME_COUNTER=0、
   RTC-onlyはSD非公開とする。GPU/4CPU/touch/battery/buttons/sharedIO等の既存definesは維持。

## 再現可能な準備

機種helper `prepare.py --out <作業root内の新規directory>`で候補AMLをiasl compileし、
固定Windows sourceとのbytes一致、source/AML/tool hash、未公開/未保持/未実機状態をmanifestへ出す。
FD/boot生成やcanonicalの変更は行わない。これはPnP/power実装の完了ではない。
