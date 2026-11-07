# RTC実値をseedにするcontinuous-S0 runtime clock候補

既定OFFの追加flag `A04BR3_MT6392_RTC_RUNTIME_COUNTER=1`。
これはEBS後のlive MT6392/PWRAP直読ではない。39bbeの検証済み実RTC date/timeをseedに、
ArmReadCntPctの経過秒でGetTime(Time,NULL)を進行させる機種限定候補である。
WACS2をruntimeへ持ち込まず、既存Windows親のPWRAP所有を変更しない。
Capabilities/SetTime/保持電源/suspendを含む完全RTC対応の完成品とは扱わない。

## 根拠と範囲

- MT6392 calendar/year1968/7bit/rolloverは固定Linux rtc-mt6397.c/mt6397-core.c。
- 同じLinux mt8167.dtsiのarchitectural timerは13MHz。
- 機種ArmV7ArchTimerSupport.SのArmReadCntPctはCP15 physical counter、ArmReadCntFrqは
  CNTFRQを読む。CNTVCT/OSのvirtual offsetやCPU cycle countは使わず、registerも書かない。
- 現機種FADTはHW_REDUCED_ACPIだけを指定し、機種ASLには_S3/_S4/_LPI等のsleep経路がない。
  これはcounterのsuspend連続性を証明するものではないため、候補は連続S0用とする。

## source動作

runtime-counter flagはRTC_READ=1を要求する。bootではRTC real readのたびにcalendarと
physical counterをanchorにし、BeforeExitBootServicesで最後に再取得する。
最後のRTC取得・INIT_DONE/CBUSY/calendar/frequency検査失敗ではanchorを無効化して渡す。
BeforeExit callbackはboot所有の専用readへ入り、失敗したEBS試行が既にruntime flagを
立てていた場合もruntime getterと混同しない。public runtime entryは常にPWRAPへ入らない。
実RTCが2014なら2014をseedにする。host time/compile timestamp/NV fake epochは使用しない。

EBS後はseedが有効で、CNTFRQがLinux値13000000と一致し、CNTPCTがanchorから退行していない
場合だけ日付と経過秒を計算する。無効seed/frequency変化/counter退行はEFI_DEVICE_ERROR。
frequency変化/退行後はruntime中に勝手な再seedを行わず、失敗を保持する。
日/月/閏日を進め、2095年末を越えたら失敗。nanosecondは0で、RTC seedに存在しない
fractional phaseを捏造しない。RTC読出し遅延と1秒未満のphase不確定性は残る。

BeforeExitの直前readは失敗時もruntime clockを無効化する。古い成功anchorへfallbackして
不確かな時刻を返さない。SetTimeはEFI_UNSUPPORTEDで、software anchorだけを書き換えて
RTCへ永続化したように成功を返すこともない。

## runtime MMIO/VAとの整合

GetTimeのruntime branchはArmReadCntFrq/ArmReadCntPct、runtime imageに常駐する値型anchorと
integer演算/CopyMemだけを使う。PWRAP MMIO、gBS/TPL、event handle、boot allocation、
DEBUG/UARTには触れない。runtime imageのcode/data relocationは既存共通RuntimeDxeに任せる。

runtime中に使うsaved MMIO baseや外部physical pointerがないため、新たな
EFI_MEMORY_RUNTIME MMIO予約やVirtualAddressChangeでのEfiConvertPointer対象もない。
VA eventを登録しただけでfixed physical WACS addressを有効とみなす実装ではない。
共通コード/RuntimeDxe/メモリmapは編集しない。実OSのSetVirtualAddressMap後実行は未検証。

RT propertiesはこのflag時だけGET_TIME bitを立てる。ただしCapabilities非NULL照会は
引き続きEFI_UNSUPPORTED。保証ppmがないため架空のAccuracy=0/MAX_UINT32を返さない。
このoptional queryまで要求するloader/clientでは採用できない点を残件として明示する。

## buildと検証

既存profileの全flagsに次の2つを加える：

```
A04_DIAGNOSTIC_MT6392_RTC_READ=1
A04_DIAGNOSTIC_MT6392_RTC_RUNTIME_COUNTER=1
```

sourceのdefaultは両方0。counter専用library INFを選び、common RTC driverは変更しない。
manifestへcounter/live-PMIC=false/suspend-compensation=false/caps=falseを記録する。
元39bbeのboot readだけをbuildした24377acfのFDは、このruntime候補の成果物ではない。

実libraryとshared bounded PWRAPを使うhost試験：

```
TMPDIR=<作業root内> RTC_COUNTER_TEST=1 python3 Platforms/Benesse/a04br3Pkg/Tests/check_mt6392_rtc.py
TMPDIR=<作業root内> python3 Platforms/Benesse/a04br3Pkg/Tests/check_mt6392_rtc.py
```

counter modeはhandoff後gBSをNULLにし、全MMIO mockをruntime accessで失敗させ、
経過秒/分/日/閏日、実2014維持、handoff失敗、frequency変化/退行の失敗保持、
year上限、caps/SetTime未対応、event登録cleanupをASan/UBSanで確認する。
これは本当のVA relocationを実行した試験ではなく、runtime branchの依存を検査する試験。
元boot modeのread/invalid calendar/rollover/timeout/RT table試験も維持する。

未確認：実CNTPCTの全S0条件での連続性、deep idle/suspend/resume、clock accuracy、
Windowsによるruntime呼出し/採用とVA map後の実行。現状はS0候補のsourceとlocal buildの段階。
live RTC直読、suspend補償、Capabilities精度、永続SetTimeは引き続き別の未完了工程。
