# ARM32 3 CPU有限時間起動診断

## 既存ESPを保持する主担当向け手順

空ESP切替後の旧4 CPU試験では最初のUART resetから約90秒fastbootが現れず、
UEFI Shell停留の後、2回目のUART resetで復帰した。これは候補bootの結果では
ない。同条件を自動で繰り返さない。`fastboot boot` もこの機種ではinitrd引継ぎ
不正の既存観測があるため使わない。

`scripts/run-boot-only-diagnostic.py` は、主担当がfastboot状態と外部reset主体の
排他を整えた後に実行する。候補と対応するboot manifest・firmware manifest、
既知の復元bootとそのSHA-256、継続収集されているUARTログを指定する。候補の
boot領域書込みとrebootの後、fastbootを一度離れたことと同一機種のfastboot
復帰を両方観測した場合に限りboot領域を復元する。ESPを操作しない。
復元コマンドの成功はreadbackではない。タイムアウト、コマンド失敗、信号、
不明状態ではresetや復元を自動実行せず、主担当が状態を確認する。

```
python3 Platforms/Benesse/a04br3Pkg/WindowsArm32/scripts/run-boot-only-diagnostic.py \
  SERIAL CANDIDATE_BOOT BOOT_MANIFEST FIRMWARE_MANIFEST \
  RESTORE_BOOT RESTORE_SHA256 UART_LOG OUTPUT_DIR
```

`OUTPUT_DIR` は新規パス。`state.json` と `commands.log`、開始位置以降のUART
スナップショットを残す。ロックはこのスクリプト間だけのホスト内排他であり、
systemd、別ホスト、手動・ハードウェアresetを排除しない。試験中にディスク
保守やDISMを開始しない。診断のReadyToBoot・ExitBootServices停止は実機未検証。

## 早期UART

FDコピー前の検査失敗は `A04FDT FAIL=0xNN` を送る。`10` はFDTポインタの
範囲・アラインメント、`01`〜`09` はそれぞれmagic、ヘッダー範囲、予約マップ、
予約衝突、構造、予約node、initrd property、memory reg、最終整合性のエラー。
続いて `A04FDT RECOVERY=RTC_WDT_ARMED` または `FAILED_HALT` を送る。UARTが
未準備・DLAB有効・送信不可のときはbounded pollingで出力を省き、出力がない
ことを成功と扱わない。RTCフラグの読戻し前にwatchdogを新規設定しない。

機種BootShimの試作FDTガードは、FDコピー前にshim内の専用スタックを使い、
LK引継ぎFDTの予約・initrdを検査する。FDTが対応範囲外・不正・未解決なら、
RTC fastbootフラグの設定と読戻しに成功した場合のみwatchdogを有効化して停止
する。RTCまたはwatchdogの操作に失敗した場合は停止し、復帰を保証しない。
この経路はホストmock・リンク検査のみで、実際のLK FDT、ハードウェアのRTC・
PWRAP・watchdog、復旧は未検証。実機での成功を前提にしない。

`A04_DIAGNOSTIC_THREE_CPU=2` はディスク保守とは別の実験用設定。CPU1を
workerに予約し、CPU0/2/3のMADT・SSDTとparkingを試す。旧edk2-mtkの
実機WinPE実績はこの構成の証拠ではない。

DXEはCPU1のfeed書込みカウンタの2回の変化を待ち、既存watchdogが有効なら
停止する。RTCのfastbootフラグを設定・読戻しできた場合だけTOPRGUを31秒で
有効化し、enable読戻しとさらに2回のfeed書込みを確認する。RTCフラグは実機の
永続状態を変更する。workerのfeed期限はCPU_ONから120秒で、期限後は31秒以内
を目安にwatchdogがresetし、RTC設定でfastbootに戻る設計である。許可された
主担当のUART `A04BR3!RST` でも期限内にresetできる。これらは実機未検証。

CPU2/3のparking応答後、ReadyToBoot時にACPIを公開してCPU0を停止する。
ExitBootServicesにも停止ガードがある。自動でDISMを開始するESPやWindows本体の
起動媒体は使わず、主担当が診断専用の媒体と外部リセット主体の排他を管理する。
ReadyToBootが呼ばれない場合も、この経路のACPI公開は確認できない。

RTC設定失敗、CPU1起動・カウンタ停止、TOPRGUの異常や電源断ではfastboot復帰を
保証できない。RTC設定失敗はwatchdogを有効化せず停止するため、UART経由または
主担当による物理復旧が必要になる可能性がある。ユーザーデータ領域の書込みや
DISMをこの診断から開始しない。実機検証がない限り、watchdog給餌・ACPI・復旧を
成功と扱わない。

ビルダーの出力は実験FDであり、`package-boot-diagnostic.py` は入力されたstock
bootとDTBを使ってAndroid boot imageを生成し、再抽出してkernelとramdiskを照合
する。DTBは旧機種リポジトリ由来で、正本UEFIソースそのものではない。stock
boot・DTBのハッシュと出自、LK実行時のメモリ配置の検査は別途確認が必要。作成済み
`boot.img` も実機投入前の試作品として扱う。

`df465ebd` の実機診断では、FDT guardの後にDXEのIRQ例外らしい出力が出てfastbootへ戻らなかった。従来はPrePiの `PlatformInitialize()` がwatchdogを停止し、後のBootTraceDxeまで再武装しないため、その間の障害をカバーできなかった。有限診断構成だけはPlatformInitialize直後にRTC fastbootフラグの設定・読戻しを確認して31秒watchdogを武装し、DXE到達後はCPU1 workerのカウンタ変化を確認して再武装する。最初の31秒以内にworkerが給餌を開始しなければfastboot復帰を試みる。新しい順序の実機動作・復帰は未検証であり、RTCやTOPRGU障害時の復帰は保証しない。
