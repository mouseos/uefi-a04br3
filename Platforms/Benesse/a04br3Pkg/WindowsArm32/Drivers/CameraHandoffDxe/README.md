# A04BR3 camera pinned handoff (既定off)

`-D A04BR3_CAMERA_HANDOFF=1`時だけ、機種固有DXE/AMLとcamera MMIO mapを含む。
Common EDK2は変更しない。既存I2C1HandoffDxe/MOU0010は変更しない。

正規CT3_GPL_LGPL_Src `7c4782127d4ea11965be00ce1343056be9bfb769`の
mtk-scpsys-mt8167.c (ISP bit5/control0x238/SRAM)、clk-mt8167.c
(CAMTG/USB_PHY48M/MM/IMG gates)、pinctrl-mt8167.c/mt8167.h、mt6392-regulator.c/
registers.h、a04br3.dtsの実HI556分岐を参照。
Linux機種driver `mouseos/a04br3-linux-drivers`固定
`3d2c74e252d6f34a5980deda96bdc04064108d74`のcamera-clockと実測記録から、
SENINF1 bit0とmux bit31、TG1 divider1/1・polarity0・24MHz手順を照合した。

- 新PLL周波数設定を行わず、既存UNIVPLLがenabledか確認する。
- GPIO48/49をreset/PDNとして準備。GPIO52/53、I2C1 clock/timing/FIFOには触れない。
- ISP domain/SMI prerequisiteを正規順序と実ACKで準備。有限100ms wait。
- CAMTGをUSB_PHY48Mへ選択、IMG capture gates、GPIO102 CMMCLK/6mA、SENINF/TGを設定。
- stock CKGEN meterのCAMTG実count960 (=48000kHz)を確認し、meter状態は直ちに復元。
- VCAMIO1.8V、VCAMA2.8V、VCAMD1.2Vをreadback確認。既にenabledなVCAMDが別電圧なら拒否。
- PDN operating、1ms、RESET解除、1ms。SYSIRQ SPI180/183を同型MT8167のpolarity反転へ。
- 成功時だけNVS marker/version/complement/PLL fingerprintをpublish。

これはboot時に電源/clockを固定して引き渡す限定候補。Windows側は第二の
TOP/GPIO/PWRAP writerを持たず、power leaseは実handoffが維持された期間だけ有効。
元の状態へのpower-off/PLL rollbackやsleep/resumeを提供したことにしない。
失敗時はmarkerを出さず、部分的に有効化したcamera資源は勝手にpower-cycleしない。
電源enable/readbackは実電圧測定やIC IDの代替ではない。IDはWindowsのMMA単一ownerで確認する。

MOU0020の_CRSはISP/SENINF一体0x15004000/0x9000、MIPI analog page0x10011000/0x1000、
LARB1 0x15001000/0x1000、NVS、IRQ212のみ。I2C1/TOP/GPIO/PWRAPは割り当てない。
_CCA=0。HSTAはNVS、domain/gates、PLL fingerprint、GPIO、TG状態を検査し、
共有設定が失われたresumeでは拒否する。camera sensorやDMAの動作成功は示さない。

`python3 scripts/generate-camera-aml.py`で自分のASLからAML/headerを再生成する。
全中間物はbuild/camera-handoff内。今回確認はiasl(0 errors/warnings)とARM32 C参照compile。
FD全体のbuildと実機動作は未確認。既存GPU通常OS試験/REA/端末には介入していない。

## 後続：ISP DMA停止と統合RAW候補


`A04BR3_CAMERA_HANDOFF=1`で機種専用camera DXE/SSDT/MemoryMapだけを有効にする。
共有EDK2/Mu、既存通常OS FDには変更しない。front cameraを追加しない。

board Linux3d2c74eのHI556電源/clockとCT3 vendor7c478212のscpsys/clock/PMIC/GPIOに対応。
共有PLLを再programせず既存enabled/PCWを検証し、CAMTG48MHzを実meterで測定後、
dividerによる24MHz MCLK、実rail ACK、reset/PDN、I2C以外のcamera pinを設定する。
GPIO48/49は16pin bank3（DIR030/DOUT130、bits0/1）。旧32pin bank計算版を使わない。
I2C1/GPIO52/53/そのtiming/clockはMMA ownerに残す。

ISP domain/gates成立後、センサーreset/PDNがassert中に旧Pass1を停止する。
TG VFDATA/CMOSとDMA/interruptをdisable、software reset request→実ACKbit1→
DMA reset status clear→hardware reset pulseを実行。DMA_EN/VFDATA/CMOSの停止を
readbackできない場合はNVS成功markerをpublishしない。

成功時だけNVS magic4134434d/version1/complement/error0、PLL PCW、実CAMTG48MHz、
model0556をpublish。HSTAは現在のpower domain/gates/PLL/pin/TGを検証する。
Windows driverはこのNVSとHSTAの両方を実検証してpower leaseを発行する。
NVSだけのbooleanを動作証拠とせず、失敗状態でdeviceを有効にしない。

MOU0020へ渡すのはcamera-private ISP/SENINF、MIPI analog、LARB1、read-only NVS、IRQ212。
shared TOP/GPIO/PWRAP/I2C1はWindows cameraへ割り当てない。
Windows側はcamera railをboot中pinし、省電力完成とは扱わない。

build helperでは`A04_DIAGNOSTIC_CAMERA_HANDOFF=1`を明示する。
`build-diagnostic.sh`の通常flagsと併用し、manifestにcamera flagと全camera source/AML hashを記録。
camera以外のnormal boot設定を落としたFDを、現通常OS候補の代わりに使わない。
生成AMLは`scripts/generate-camera-aml.py`で再現し、ASL/AMLの手編集を正式buildへ混ぜない。

Windows統合実装はa04br3-windows-driversのwork/hi556-capture-candidate。
WDK build/署名と実非ゼロframe成功は別。Linux元経路もNCSI2→MUX以後が未解決であり、
このhandoffやresetだけで撮像問題が解決したとは主張しない。実機は主担当のみが操作する。
