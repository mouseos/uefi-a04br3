# USB・audio・Wi-Fi同時profile

`A04BR3_SHARED_IO_PROFILE=1` を指定すると、BEN0001の既存5領域と子PDO契約に合わせたACPIと、USB/AFE/MSDC2のSYSIRQ handoffを選ぶ。既定0。機種パッケージだけの変更で、EDK2共通部を変更しない。build scriptでは `A04_DIAGNOSTIC_SHARED_IO_PROFILE=1` を使う。

共有親/APMIXED/battery/GPU/display/shared touch powerを有効にした既存baselineが必要。legacy touch providerとの競合は既存guardで拒否する。profileをOFFにすると既存のACPI選択へ戻り、新しいSYSIRQ driverを含めない。

## 所有範囲

ACPIの `SHR0` はHID `BEN0001`、UID0、`_CCA=0`。親の `_CRS` は次の5メモリ領域だけ。IRQや子のメモリを親へ重複追加しない。

| 親専有領域 | base | bytes |
| --- | --- | --- |
| TOPCK | 0x10000000 | 0x1000 |
| INFRA | 0x10001000 | 0x1000 |
| GPIO | 0x10005000 | 0x1000 |
| PWRAP | 0x1000f000 | 0x1000 |
| APMIXED | 0x10018000 | 0x714 |

子は同じdrivers repoの共有親が生成する**synthetic PDO**。ACPI子を作って二重列挙しない。

| 子PDO ID | 専有メモリ | IRQ |
| --- | --- | --- |
| MT6392\Buttons | 0x10002000/0x1000 | 追加なし |
| MT8167\BU21025 | 0x11009000/0x1000 | 追加なし |
| MT8167\USB20 | MAC 0x11100000/0x10000、SIF 0x11110000/0x10000 | SPI72 → GSI104 |
| MT8167\AFE | 0x11140000/0x1000、0x11141000/0x9000 | SPI120 → GSI152 |
| A04BR3\MT6630SDIO | MSDC2 0x11170000/0x1000 | SPI109 → GSI141 |

USB/audio統合親 `d9d95dc` とWi-Fi `c29fcbf` の `ChildRequirements` はこの範囲・GSI・LevelSensitive契約で一致する。audioは `A04_AUDIO_CHILD_RESOURCES=1`、Wi-Fi IRQは `A04_SHARED_WIFI_IRQ_DIAGNOSTIC=1` が必要。Wi-Fi runtimeはさらに `A04_SHARED_WIFI_RUNTIME=1` とRTC32K lease等の実装が必要。

`d9d95dc` のUSB/audio親と `c29fcbf` のWi-Fi runtime親は、同じファイルでも機能差分がある。USB/audioにWi-Fi runtimeのRTC32K・世代lease実装を統合した**1個の親SYS**を使用する。USB用とWi-Fi用のBEN0001親SYSを2個インストールしたり、後から片方で上書きしない。UEFI側が同じ資源を記述しても、別branchのdriver実装が自動的に統合されるわけではない。

既存のbattery/I2C2 `0x1100b000`、通知用button array MOU0002、GPU MOU0006、display MOU0007、eMMC MOU0001、touch powerとそのhandoffは維持する。既存ASLは変更せず、profile用INFはSharedParentIo.aslだけを差し替える。別のlegacy USB/Wi-Fi/AFE/touch ACPI fragmentを追加しない。Wi-Fi combo GPIO5/EINTの割込みはMSDC2と別物なので追加しない。BT/FM/USB1等も今回の対象外。

## SYSIRQとHALの一組の契約

一次ソースは [CT3 GPL/LGPL source 7c478212](https://github.com/s1204IT/CT3_GPL_LGPL_Src/tree/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4)。`a04br3.dts` が `mt8167.dtsi` をincludeし、SoC側のinterrupt-parentはSYSIRQ。USB72、AFE120、MSDC2 109、GPU185はいずれもIRQ_TYPE_LEVEL_LOW。

`irq-mtk-sysirq.c::mtk_sysirq_set_type` は `intpol_base + (SPI >> 5)*4` の `SPI & 31` をsetし、親GICへ渡すtypeをLEVEL_HIGHへ変換する。`irq-gic.c::gic_set_type` はSPIについてLEVEL_HIGHかEDGE_RISINGのみを受け付ける。GSI番号へ32を加算してからSYSIRQ bitを算出してはいけない。

| 入力 | GSI | SYSIRQ register | mask | GIC/HALへ渡す信号 |
| --- | --- | --- | --- | --- |
| USB SPI72 low | 104 | 0x10200628（MCUCFG +0x628） | bit8 = 0x00000100 | level-high |
| MSDC2 SPI109 low | 141 | 0x1020062c（+0x62c） | bit13 = 0x00002000 | level-high |
| AFE SPI120 low | 152 | 0x1020062c（+0x62c） | bit24 = 0x01000000 | level-high |
| 既存GPU SPI185 low | 217 | 0x10200634（+0x634） | bit25 | 既存GPU driver/ACPIのlevel-highを維持 |

新しいSharedIoIrqDxeはBeforeExitBootServicesで前2wordをread-modify-writeして3bitだけsetし、readbackをUARTへ一回出す。GPU wordや他のIRQのbitは変更しない。IRQ enable、GIC distributor、USB PHY、PMIC、PLL、audio、Wi-Fi電源へは書かない。readback不一致は明示したエラーとして残し、配送成功とみなさない。

synthetic PDOの [IO_RESOURCE_DESCRIPTOR](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_resource_descriptor) には極性fieldがない。driversのCM_RESOURCE_INTERRUPT_LEVEL_SENSITIVEと、SYSIRQ反転後の**GIC ActiveHigh**を対として使う。ACPIにActiveLowの重複デバイスを作ってHALへ設定を渡そうとしない。HALに架空の極性fieldを追加しない。今回のACPIは親メモリとnamespaceの整合を提供するもので、実Windows HALが採用した割込み設定・実配送を観測した証明ではない。必要なら主担当の実機工程でraw/translated resourcesと配送を確認する。

## 検証・出自

`Acpi/shared-io-contract.json` に親/子/IRQの固定契約、USB98775b4→d9d95dc・Wi-Fi c29fcbfの完全commit、Linux source commitとpath、未検証事項を記録した。ビルドmanifestはflagとcontractの本文・SHA256を含む。boot manifestにもfirmware_build_definesでflagを引き継ぐ。

```
TMPDIR=<作業ルート内のtmp> python3 Platforms/Benesse/a04br3Pkg/Tests/check_shared_io_profile.py \
  --drivers <a04br3-windows-drivers> --linux <CT3 kernel-4.4> --out <作業ルート内のログ先>
```

iaslで全profile ASLをコンパイルしAML checksumを検証、acpiexecでBEN0001 `_CRS` の5領域・`_CCA=0`・button arrayを評価する。子メモリとACPI各所有者の重複がなく、driverの固定GSI/範囲とDTSが一致することを確認する。実DXE callbackを模擬MMIO上で実行し、2word以外への書込みがなく、他bit保持、一回限り、readback失敗を検査する。

成果物生成と実機成功は別。今回、実機・SSH・REA操作は行わない。USB/audio/Wi-Fi同時PnP、割込み配送、各driverのpower/clock lease競合、resume、実入力/再生/無線接続は未検証。
