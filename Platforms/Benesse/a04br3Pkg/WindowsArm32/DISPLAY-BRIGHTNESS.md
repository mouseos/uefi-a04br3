# MT8167 DISP_PWM0のWindows資源

既定OFFの`A04_DIAGNOSTIC_DISPLAY_BRIGHTNESS_RESOURCES=1`を機種build scriptへ追加すると、
既存DIS0/MOU0007の_CRSに`0x1100f000/0x1000`のDISP_PWM0専有窓を追加する。
GPU provider、BEN0001、GPIO、TOPCK、MMSYS、PMIC、既存display4領域とIRQは変更しない。
EDK2共通部の変更はない。ASL preprocessor defineとfirmware manifestのキーは
`A04BR3_DISPLAY_BRIGHTNESS_RESOURCES`。display providerが有効な場合だけONを受理する。

根拠はCT3 Linux `7c4782127d4ea11965be00ce1343056be9bfb769`の
`kernel-4.4/arch/arm64/boot/dts/mediatek/mt8167.dtsi`にある
`mediatek,mt8167-disp_pwm`のreg。既存共有parentはGPIO窓を所有するため、
brightness driverがGPIO1のbacklight enableを重複所有する資源は追加しない。

Windows側の対応branchはa04br3-windows-driversの`work/mt8167-disp-pwm-brightness`。
標準DOD brightness interface v1とDISP_PWM dutyを接続し、既存running PWMの1～100%を調整する。
Windows側flag `A04_MT8167_DISPLAY_BRIGHTNESS=1`も必要で、旧UEFIで未割当アドレスをmapしない。
新FD/bootの正式build・Windows package署名・実機確認は主担当で実施する。

既存baseline設定とbuild scriptの5引数を維持してflagだけを追加する。
TMPDIRは作業root内の専用directoryを明示する。生成manifestでflag/ソースcommit/hashを確認する。
本変更だけのために既存GPU/shim/watchdog/CPU設定を落とさない。

検証:

```sh
python3 Platforms/Benesse/a04br3Pkg/Tests/check_display_brightness.py \
  --out "$TASK_ROOT/port/gsi/logs/ios-feasibility/mt8167-brightness/acpi"
```

brightness OFF/ON × GCE OFF/ONの4組をiaslでcompileし、AML checksum、既存4領域保持、
PWM領域のON限定出現とMMIO重複なしを確認する。実機/runner/REA/カメラは使用しない。

ACPI格納fileの検証はGUID定数を再掲せず、実buildのGNUmakefileから選択INFを取得し、
そのFILE_GUIDと使用した上流MdeModulePkg.decのPcdAcpiTableStorageFileをbytes_leで比較する。
さらに生成済みAcpiPlatformDxe PCDとFFS存在を照合し、結果をfirmware manifestへ含める。
不一致の旧cc7/profile1・未修正18d成果物は適用禁止。
