# Parent1022：GPU1014 + SD1 + 実thermal PIO単一owner

基点GPU親287241c37fa0eae538e5e2d136dea5df42de7e43（ecaa Audio forwarding + SD1010 ancestry）へ
thermal1018/7ac48ff実装を統合。旧GPU1014/thermal1018署名SYS/bootは変更しない。
単一ACPI BEN0001/SHR0へ親1022だけをbind、同じMt8167Shared service名を更新する。
別親1018を並行bindしない。SD1は必須、親INF2026.11.7.1022、子thermal0.0.1.6。

新既定OFF A04_SHARED_GPU_THERMAL_PROFILE=1はGPU_PLATFORM1/GPU_DOMAIN_SPM1/SD_POWER1/
THERMAL_DIAGNOSTIC1/NATIVE_EXPERIMENT1を要求。既存GPU1014 profileの全flagsを維持し、
thermal変更2flags＋combinedflagだけ追加。旧1018のSD0 recipeはこの新sourceでは拒否し同版異SYSを作らない。
GPU domain owner/SD power/AFE forwarding実装は287からbyte不変、既存parent mutex/StopIdle/leaseを使用。

機種ACPIの既存6rangeへthermal0x1100d000/1000とAUXADC0x11003000/1000だけ追加。
正規raw/translated排他8rangeを厳密照合し、元6rangeは旧GPU parserへ委譲する。
thermal/ADCはidentity translatedを要求し、重複/短range/readonly/未割当は拒否。
親はSPMを既存gpuSpmで一回mapするだけ。thermal PDOが正規2rangeを一度mapし、
Measure Adopted1で同親へ所有を移し、子は以後アクセス/freeしない。親が別のSPMをmapしない。
IRQ109を新たに登録せず、旧thermal AHB/IRQ producerは機種BeforeEBSでmask/idle/pause/disable/reset保持。

LKのcurrentboot devinfo word60/61/62→HOB→volatile校正v1/16bytes attrs6、
停止record v2/18 UINT3272bytes attrs6は旧95216ec2と同じ契約。
親が今回bootのrecordを一回消費し、live TOP/reset/IRQ/AHB/ADC mode/全trigger/busyを再検証。
親mutex下でだけnative ADC leaseを立て、legacy thermal/codec ADCはlease/native_claimで排除する。
既存PMIC AUXADC（別PMIC）とSoC AUXADCを混同せず、新PWRAP/PMIC値を増やさない。

TS_CON1 sensor0/1/2、200us buffer settling、channel11 oldready0→trigger→25us→freshready1、
実LK校正の3点を取得し実PnP D0からUART/readoutへ公開。レジスタsnapshotだけの成功stubではない。
測定/cleanup総期限500ms＋既存各poll有限。期限超過はアクセス資格を閉じ、
停止/analog復元/reset再確認が未証明ならmapping/親object/電源参照をquarantine保持する。
通常ADC timeoutでcleanup/analog restore/reset保持が確認できた場合も測定失敗で返し、温度公開なし。
panic/ResumeIdle/free成功偽装なし。thermalだけpoisonしGPU/SD/audioのledgerを消さない。

一次根拠: CT3 kernel4.4 drivers/misc/mediatek/thermal/mt8167/mtk_ts_cpu.c（probe reset/init、
suspend mask/pause/AHB idle）、同auxadcのIMM_auxadc_GetOneChannelValue、mtk_thermal.h/
mt8167.dtsi。ADC coreは元GPL-2.0-only表記保持。GPU Linux根拠/bit許可は287のまま。

build: tools/ci/build-gpu-thermal1022.ps1 -CertificateThumbprint <existing BCBF>
UEFIは6e852+95216ec2の機種merge、default-off GPU_THERMAL_PROFILE1でだけ8rangeを公開。
10表中共有親SSDTだけ差分、他9表/CPU4/WDT/FB/SD/brightness/GPST/IRQを保持して確認する。
実測温度/clock共存/SD/GPU/suspend/removeはmain実機未検証。一回測温資格候補であり
continuous thermal protection/高クロック制御の完成とはしない。実機main、初回GPU試験へ混入しない。
