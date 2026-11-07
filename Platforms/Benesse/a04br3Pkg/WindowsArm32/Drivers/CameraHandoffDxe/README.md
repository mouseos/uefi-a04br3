# HI556用camera handoff（既定off）

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
