# MT6392 RTC calendarのboot読出し

別の既定OFF RTC_RUNTIME_COUNTER候補はRTC-COUNTER-CLOCK.mdを参照。
本文のboot-only仕様はRTC_READ単独のもので、live RTC直読やcapsの制約は変えない。

実装範囲は機種package内の既定OFF `A04BR3_MT6392_RTC_READ=1`。
既存RealTimeClockRuntimeDxeのlibrary bindingだけを機種Mt6392RtcLibへ替える。
共通EDK2、既存PwrapRead.h、GPU/4CPU/ACPI GUID/brightness/touch/battery/buttons/
sharedIO、boot配置には変更を加えない。341894cdをbaseに別worktreeでsourceだけを作業し、
canonicalのcheckout/HEAD/dirty、FD/bootは変更・生成していない。

## 実測と原因の区別

ユーザー申告は実Windows RTCが2014/1/18に固定。機種WindowsSilicium.dsc.incは
RealTimeClockLibNullを選んでおり、読んだNullソースはyear=2019/month=1と
counter由来dayを作っている。実RTCを読まない構成であることは確定するが、この
定数と実測2014/1/18は一致しない。実行FD由来・loader側fallback・MT6392実値の
どこで2014になるかは未確定。2014を見ただけでRTC故障やNullだけが原因とは断定しない。
実RTCが本当に2014なら、今回もその実値を返す。host日時へ補正して成功扱いしない。

## 一次ソースと読出し

CT3 Linux commit 7c4782127d4ea11965be00ce1343056be9bfb769の
`drivers/mfd/mt6397-core.c`はMT6392 RTC base=0x8000、
`drivers/rtc/rtc-mt6397.c`はcompatible mt6392とTC_SEC offset0x0a、
SEC/MIN/HOUR/DOM/DOW/MTH/YEARの7個・2byte間隔、yearbase1968・7bitを定義する。
月は1始まり、year範囲1968～2095。read_timeは7register後にSECを再読し、
secondが小さくなった時だけcalendarを読み直す。

機種共通のTouchPower由来PwrapRead.hをそのまま再利用する。
WACS2 INIT_DONEを確認し、RTC_BBPU.CBUSYがclearの時だけ7fieldを読む。
SEC再読によるrollover再試行は最大3回、各transport waitは従来通り10000回/1us。
SEC/MIN/HOUR/month/yearの範囲、Gregorian日付・閏日を検査し、無効値を
maskして有効値へ作り替えない。DOWはEFI_TIMEに含まれず、Linuxも再計算するため
生logに残すだけで日時判定には使用しない。BBPUを最後にも確認する。
同一UEFI CPUの他callbackとWACS2 commandが混ざらないようboot中だけTPLを上げる。

失敗時はEFI_DEVICE_ERROR、Timeをfake dateで上書きしない。
成功時のみTC実値をEFI_TIMEへ変換し、Nanosecond/pad/daylightを0、Timezoneを
EFI_UNSPECIFIED_TIMEZONEとして返す。RTCにはtimezoneがなく、UTC/JST変換は推測しない。
最初の実読出しだけDEBUG_ERRORにstatus、BBPU、生7field、SEC再読、attemptを出す。
WACS read command/valid-clearのMMIO書込みはあるが、PMIC register write commandはゼロ。
RTC PROT/WRTGR/PDN/SPAR/IRQ_STA、PLL、rail、charger、32K設定は書かない。

このvalidityは**transport・coherence・calendar構造の判定**だけ。
MT6392一次ソースで非破壊に判定できるbattery/oscillator loss flagは今回確立できなかった。
他PMIC mt6335/mt6351のCON/LP bitを推測で流用せず、read-to-clearのIRQ_STAも読まない。
logはloss_validity=unknownとする。正確な現在日付、clockの進行、保持電源は実測未確認。

## 一般UEFI runtimeとWindowsの要件

[UEFI Runtime Services 2.10](https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html)は
EBS後に使えないserviceについてEFI_RT_PROPERTIES_TABLEで通知し、無視するOSにも
callable stubでEFI_UNSUPPORTEDを返すよう規定する。これはUEFI2.8aで導入されたtableで、
旧Windows RTが参照する保証はない。callback自体がEBS後にPWRAP/gBSへ触れないことを
runtime guardで保証する。物理MMIOをvirtual runtimeから直読する実装にはしない。

[Microsoft SoC UEFI requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/uefi-requirements-that-apply-to-all-windows-platforms)
はGetTime/SetTimeを要求し、time hardwareへの呼出しはEBS前と記す。
ただし現在の資料の適用対象はWindows10 desktopであり、これだけをWindows RT8.1の
全契約・全呼出し時点の証拠にはしない。今回Windows実機の呼出しは未観測。

実装はboot GetTime(Time,NULL)のread-only subsetであり、**Windows RTC完全対応ではない**。
SetTimeとwakeupは未実装でEFI_UNSUPPORTED。UEFI RTC Accuracyの単位は1e-6 ppmで、
MT6392実測/仕様の誤差上限が未確認のため、0=完全精度や架空ppmを返さず、
Capabilities非NULLのGetTimeもEFI_UNSUPPORTEDとする。bootloaderがCapabilitiesを
必須とする場合、このsubsetでは時刻を採用できない。対照実測が必要。

EBS後はWindows共有親がPWRAP/WACS2を所有する。firmwareとの共通lock/handoffがないため、
時刻readであってもruntime WACS2 commandを発行しない。GetTimeもEFI_UNSUPPORTED。
RT propertiesは既存tableの他service bitsを保持してtime4bitだけclearする。
既存tableがなければ従来のimplicit all-supported maskを引き継いで同じ4bitをclearする。
これは他runtime servicesを新しく実装・検証した主張ではない。tableはruntime imageに常駐。
ハード不在でもNullにfallbackせず、RTC architectural protocolのgetterが失敗を返す。

未完了は持続的SetTime、アラーム、power-loss validity、精度caps、Windows側RTC/TADまたは
firmware共通ownerの統合、実boot読出し/OS時刻採用/進行/再起動保持/resume。
OS側の時計設定・校正を済ませたようには扱わない。hostclockを書き込む手順も追加しない。

## flagと次のbuild条件

sourceのdefaultは0。既存profileの全flagに対して次だけを追加する入口を用意した：

```
A04_DIAGNOSTIC_MT6392_RTC_READ=1
# -> -D A04BR3_MT6392_RTC_READ=1
```

build-diagnostic.shのmanifestにdefineとreadonly/boot-only/caps/set/wakeup/validityの
範囲を記録する。0では従来のlibrary選択を維持する。build/checkout切替/flashは
ユーザー指示に従って今回実行していない。canonicalからの後続FD生成は主担当工程。

## host検証

`TMPDIR=<作業root内> python3 Platforms/Benesse/a04br3Pkg/Tests/check_mt6392_rtc.py`
は実library Cと実PwrapRead.hを模擬MMIOで実行する。正常変換/実2014の保持、閏日、
各無効field、秒rollover/再試行打切り、INIT未完了/transport timeout/CBUSY、
出力保持、無書込みSet/caps/unsupported、runtimeでMMIOとboot servicesゼロ、
RT table mask保持/不正table/登録失敗、RTC不在時の非fallbackをASan/UBSanで確認。

実EDK2 headerと既存機種ARM compatibility headerでもclang -fsyntax-onlyを確認した。
bash構文/embedded Python AST/default-off bindingも確認。firmware compile/link、
FD/boot生成、実RTC読出し、Windows実時刻は未確認。実機/runner/REA操作なし。
