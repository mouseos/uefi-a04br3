# RTC runtime直読の成立条件と未完了点

この本文は24377acf時点のlive RTC直読調査。後続の別候補（実RTC seed＋physical counter）は
RTC-COUNTER-CLOCK.mdに分離した。以下のPWRAP直読/Capabilities/SetTimeの未解決点は維持する。

39bbe10a後の調査。現時点でEBS後GetTime/Capabilitiesは**未完成**。
共通EDK2/canonical/実機を変更せず、39bbeのruntime refusalを維持する。
この記録やhost counterexampleをruntime時刻修正の完成品として扱わない。

## mapping/VAとハード所有は別の条件

機種WindowsArm32/Library/MemoryMapLibのPWRAP 0x1000f000/0x1000は
AddDev/MMAP_IO/UNCACHEABLE/MmIO/NS_DEVICEで、EFI_MEMORY_RUNTIME指定がない。
現在のPwrapRead.hはWACS_CMD=0x1000f0a0、DATA=...a4、CLEAR=...a8の固定物理値。
このままruntime guardだけを除くと、WindowsのVA map上では正しくない。

共通を変更せずにmappingを実装する方式自体は存在する。既存PL031RealTimeClockLibは
boot中のGCD runtime属性登録とVirtualAddressChange eventでsaved MMIO baseを
EfiConvertPointerする。機種側でも既存GCD descriptorのtype/範囲/capabilitiesを検査して
runtime属性を付け、literal addressではなくconverted base+offsetを使用する必要がある。
既にPWRAP窓がGCDへ登録済みなので、AddMemorySpaceの単純な重複追加は不可。
登録・変換失敗時は物理baseで強行せず失敗を返す。runtimeでgBS/TPL/allocateを使わない。

しかしmapping/VAはWACS2の排他を提供しない。これだけ実装・有効化しても
runtime RTCを安全に公開できないため、所有根拠なしにPWRAP runtime予約を追加しない。

## 実際に同一channelを使用する二つのowner

Linux固定7c4782127d4ea11965be00ce1343056be9bfb769の
`drivers/soc/mediatek/mtk-pmic-wrap.c`：

- mt8167_regsはWACS2 CMD/RDATA/VLDCLR=0xa0/0xa4/0xa8。
- pwrap_readはidle poll→read CMD→FSM6→RDATA→VLDCLR。PMICを書かないreadでも
  AP側のcommand/response stateを変更する。
- regmap経路のlockingはLinux内のもの。UEFIとWindowsに共通するlockではない。
- 初期化はWACS0/1/2とINIT_DONE0/1/2をすべて有効化する。Linux AP regmapが2だけを
  使用することは、0/1をfirmware・secure側から独占可能とする根拠ではない。

Windows sourceのdrivers/input/a04br3-buttons/pmic_read.cも
同じ0xa0/0xa4/0xa8を使い、親はOS内のmutexでPWRAP全clientをserializeする。
UEFIのTPL/firmware-local spinlock、あるいはUEFI runtime services間のnon-reentry規約は、
他CPU/既存Windows driverのこのmutexへ参加しない。

特に「IDLEを見てから使う」「INIT_DONEを確認」「RTC read-onlyだから安全」は不十分。
同じIDLEを二ownerが見てからCMDを書ける。responseには要求registerのtagがなく、
片方が他方のresponseを読み、VLDCLRで消すことをreader単独では検出できない。

`Tests/check_rtc_runtime_ownership.py`は実PwrapRead.hに対するprotocol counterexample。
正常readは23を返すが、IDLE check後に別ownerが完了するsingle-outstanding modelでは、
別応答17をEFI_SUCCESSで返し、そのresponseをclearできることを示す。
これは実機・siliconのCMD-busy挙動を測定した結果ではない。
現在のsourceが排他・response ownershipを保証しないことを示すhost modelである。

## 他方式も根拠なしに代用しない

MT6392 RTCはARMの直結MMIO calendarではなくPWRAP先のPMICで、別の直接TC窓は
このLinux sourceにない。WACS0/1専有にはfirmware/secure側を含む所有資料が必要。

RTC snapshot + ARM counter投影ならruntime PWRAPを使わずに進行させる案はある。
しかしそれをlive MT6392 readと呼ばない。mt8167.dtsiのarch timerは13MHzだが、
この記述だけで全suspend/resumeのcounter連続性・UTC保持・rate精度を証明できない。
今回未検証のcounter projectionを成功するRTCの代わりに追加しない。

Linux rtc-mt6397.cは1秒binary TC fieldsとyearbase1968を根拠にできるが、
MT6392 oscillatorの保証ppmは記載しない。UEFI Accuracy=0はunknown sentinelと定義されて
いない。MAX_UINT32をunknown扱いしても、仕様上の検証済み誤差上限にはならない。
Capabilitiesを成功させるには、保証可能な誤差上限か、別clock sourceの明示的な契約が必要。

## 成立させるために必要な追加情報・変更範囲

次のどちらかの排他根拠が必要：

1. Windows共有親の全PWRAP clientをquiesce/serializeし、firmware time呼出しの期間を
   保護するOS/firmware共通契約。単なるfirmware側lock追加では成立しない。
2. RTC runtime専用WACS channelを割当てられるfirmware/secure source根拠とhandoff。
   enable/idle bitやLinux内の利用有無だけでは所有の代わりにしない。

それとCapabilities用のprecision/rate根拠を揃えれば、機種libraryのGCD/VA実装と
runtime読出しを接続できる。現在の「UEFI機種packageだけ、既存Windows owner不変」では
これらの情報・契約が不足している。39bbeのboot readまでを維持し、未成立を明示する。

SetTimeはさらにRTC PROT/WRTGR/BBPU、全TC fieldのtransactionと失敗時の既存clock保護が
必要。共通ownerがないまま書込み・unlock・回復操作を行わない。
精度や日時をhostclockで偽装せず、SetTimeも未実装のままにする。

今回はsource調査とhost modelのみ。runtime許可flag、MMIO reservation、共通コード、
FD/boot、canonical checkout、runner、実機、REAには変更・操作なし。
