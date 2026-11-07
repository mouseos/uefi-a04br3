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
場合だけ日付と経過秒を計算する。各callで最大64回のCNTPCT readにより進行も検査する。
無効seed/最終handoff失敗はEFI_DEVICE_ERROR。frequency変化/counter停止/前回成功値からの
退行はEFI_UNSUPPORTEDを保持し、runtime中に勝手な再seedを行わない。
日/月/閏日を進め、2095年末を越えたら失敗。nanosecondは0で、RTC seedに存在しない
fractional phaseを捏造しない。RTC読出し遅延と1秒未満のphase不確定性は残る。

BeforeExitの直前readは失敗時もruntime clockを無効化する。古い成功anchorへfallbackして
不確かな時刻を返さない。SetTimeはUEFI fieldを検査し、bootでは実RTCと完全一致する
無書込みno-opだけEFI_SUCCESS。比較readでもanchorは書き換えない。それ以外とruntimeは
EFI_UNSUPPORTED。software anchorだけを書き換えて永続化したように成功を返さない。
LinuxのTC bulk_write→WRTGRは、部分write失敗後のshadow rollbackを保証しない。
PROT unlockや後続の別WRTGRによる部分commitを安全に扱う根拠がないため移植しない。

## runtime MMIO/VAとの整合

GetTimeのruntime branchはArmReadCntFrq/ArmReadCntPct、runtime imageに常駐する値型anchorと
integer演算/CopyMemだけを使う。PWRAP MMIO、gBS/TPL、event handle、boot allocation、
DEBUG/UARTには触れない。runtime imageのcode/data relocationは既存共通RuntimeDxeに任せる。

runtime中に使うsaved MMIO baseや外部physical pointerがないため、新たな
EFI_MEMORY_RUNTIME MMIO予約やVirtualAddressChangeでのEfiConvertPointer対象もない。
VA eventを登録しただけでfixed physical WACS addressを有効とみなす実装ではない。
共通コード/RuntimeDxe/メモリmapは編集しない。実OSのSetVirtualAddressMap後実行は未検証。

RT propertiesはこのflag時だけGET_TIME bitを立てる。ただしCapabilities非NULL照会は
既定ではEFI_UNSUPPORTED。保証ppmがないため架空のAccuracy=0/MAX_UINT32を返さない。
`A04BR3_RTC_ACCURACY_X1E6_PPM`の既定0は内部の「未資格」印であり、EFIへ返さない。
非zeroの資格済み設定時だけResolution=1、Accuracy=指定値、SetsToZero=FALSEを返す。
build wrapperの`A04_DIAGNOSTIC_RTC_ACCURACY_X1E6_PPM`を非zeroにするには、
`A04_DIAGNOSTIC_RTC_ACCURACY_EVIDENCE`に根拠fileを指定しhashをmanifestへ残す。
根拠fileの存在だけで精度を認定しない。MT6392と13MHz counterを含むbackendのrate誤差、
条件/測定方法をレビューする必要がある。今回の実build値は0で、実精度は未資格。
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

## S0限定の境界と観測値

`check_rtc_s0_contract.py`をcounter buildの前後へ接続し、機種sourceのsleep/idle宣言、
選択された実FADT flags/sleep registersとAMLのsleep/idle objectを検査する。
広告されたsleep経路が増えればbuildを拒否し、counter backendをOFFにしてunsupportedへ
戻すか、別の補償根拠を実装する。通常のD3 device遷移だけでsystem suspendとは扱わない。

この検査やcounterの短時間進行確認は、経過中の停止→再開を検出できる仕組みではない。
OS/platformが宣言外のdeep idleやsuspendでcounterを止め、呼出し前に再開した場合、
失われた時間はcounterだけでは検出・補償できない。その構成を本候補の対応範囲に含めない。
S3/S4/LowPowerS0/LPI等を使う通常構成の完全なruntime RTC対応は未完了である。

UARTの`rtc_calendar_valid`はcalendar/transport構造が有効、`tc_change_observed`は
read中に実SEC値の変化を見たか、`counter_alive`はcounter短時間進行、`seed_adopted`は
EBS直前の実RTC seed採用を意味する。それぞれ時計精度・保持電源・Windows採用の証明ではない。
S0での経過秒が進むこととlive RTC読出しも区別する。runtimeでのDEBUG/PMIC accessはない。

host試験は既存boot/counter harnessを拡張し、Caps出力保持、停止/前回値からの退行、
未handoff、実RTCとcounter採用の区別、SetTimeのexact-match/no-op/invalid/mismatchを確認する。
`RTC_COUNTER_TEST=1 RTC_TEST_ACCURACY=123456789`の値はhost専用synthetic値で、
Capabilities fieldの経路だけを試す。実FDの精度根拠として使わない。

## CNTPCT読出しの命令同期

固定Linux 7c4782127d4ea11965be00ce1343056be9bfb769の
`arch/arm/include/asm/arch_timer.h:arch_counter_get_cntpct`はISB→MRRCで読む。
機種ArmLibのArmReadCntPctはMRRCだけなので、RTC内のRtcReadCounterで各read前に
ArmInstructionSynchronizationBarrierを呼ぶ。共通ArmLibやtimer利用者全体は変更しない。
seedとruntimeの進行probe両方へ適用し、host mockで各counter read前のISBを要求する。
命令同期は実周波数/精度/suspend連続性やWindows採用を証明しない。
