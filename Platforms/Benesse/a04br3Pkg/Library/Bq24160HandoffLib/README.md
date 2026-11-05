# BQ24160ファームウェア所有・引継ぎ部品

このライブラリは機種固有の試作であり、I2C、MMIO、イベント、カウンタ、ACPIテーブルの実装を自身には含まない。既定OFFの搬送試作ではI2c2HandoffDxeが停止状態の所有者だけを呼び出し、BQへ通信しない。PIO処理・コールバックはソースにあるが、未実装の条件証明関数は常にfalseを返す。BQには読取りでもwatchdogを開始する副作用があり、接続前にWindowsへの受渡し、D0外・バス障害時を含む継続責任を確立する必要がある。Windows側の既存安全ゲートを解除する根拠にはならない。

## 動作

- 初期化にはすでに準備した保守タイマー、I2C2の単一所有、Windows側の受信準備、電源遷移・障害時の継続責任、および直前の所有者が定めた単調時計上の絶対期限が必要。これらは呼出し側が実際に保証しなければならず、関数の真偽値自体は保証や証明ではない。一つでも不成立ならアクセス前に停止をラッチする。新しい20秒を初回に与えない。
- 周期処理はコピーした共通ポリシーを実行し、完全な設定・状態の照合に成功した場合のみwatchdogを給餌する。電圧・電流・入力設定を書き換えない。最後の給餌の物理カウンタは書込みの直前に採取し、処理遅延で期限を実際より延長しない。カウンタ逆行、通信・ポリシー異常、期限超過は停止をラッチする。
- 呼出し側は各周期とExitBootServicesを排他し、LC709203Fを含む全I2C2利用者も同じ排他に参加させる。割込み・TPL・マルチCPU・Windows側との実装はまだない。
- EBSでは外側から同じ物理カウンタの終了値と当該起動専用の128バイト領域を受け、周期処理を停止し、最初と直近のサンプル、valid mask、FAULT履歴、成功回数、最後の給餌時刻を記録する。すべての条件を満たす記録だけ状態1とし、途中失敗・期限切れ等は非採用の状態2にして部分情報を残す。出力先がない場合も停止し、二度目は記録を変更しない。EBS処理自体はI2C・MMIO・タイマー操作・アロケーション・外部コールバックを行わない。EBS失敗による再試行を含め、停止後の自動再開はしない。
- 記録のFNV値は破損検出であり認証ではない。物理カウンタの周波数・origin・Windows側での読取り可否、recordのメモリ寿命・真正性・一回限りの所有移管を別途実装・検証する。既定OFFのWindows UEFI搬送試作は停止状態の記録のみ公開し、BQのI2C通信には接続されていない。

## 出自とホスト試験

`charger.c` と公開coreヘッダはMITライセンスの [a04br3-windows-drivers](https://github.com/mouseos/a04br3-windows-drivers) の `83535141d50a906167de1620e02949479f925c39` に由来し、UEFI向けにinclude名だけ変更した。元のポリシーは [UniXNU-a04br3](https://github.com/mouseos/UniXNU-a04br3) `0420ce37f0241178a7276784bbc013b0dbba4c95` の `A04BR3ChargerCore.h` / `A04BR3ChargerHandoff.h` を参照する。標準外のPMICやBQ設定値を新たに推測していない。

```
python3 Platforms/Benesse/a04br3Pkg/Tests/BqHandoff/test_bq_firmware_owner.py
python3 Platforms/Benesse/a04br3Pkg/Tests/BqHandoff/test_bq_firmware_owner.py --windows-driver-root /path/to/a04br3-windows-drivers
```

２番目の試験は元実装と同じ版のWindows側coreソース・ヘッダの一致も検証する。Windows側が後に変更されていれば一致しない。模擬試験はWindows側と同じ採用関数で正常記録と停止記録を検査し、未準備、partial read、失敗したkick、期限切れ、カウンタ逆行、EBS後の無操作を確認する。ARM32オブジェクトのコンパイルはFDのビルド・リンクやWindows実機動作を示さない。

## Windows向け停止記録の搬送試作

`A04BR3_BQ_HANDOFF=1` は既定OFFで、既存の `A04BR3_BATTERY_PROVIDER=1` とその前提を必要とする。`I2c2HandoffDxe` はReadyToBootで電池SSDT `A04BATT` の先行公開を検査し、確認できた場合に1ページのACPI NVSを確保し、そのアドレスへ `SystemMemory` OperationRegionを指すSSDTを公開する。`\_SB.BAT0.BQHR` は引数なしで先頭128バイトをBufferとして返す。Windowsの `MOU0008` ドライバはPDOに `IOCTL_ACPI_EVAL_METHOD` を送り、メソッド `BQHR` の戻り値の型・長さ128・記録のversion・seal・state・期限を検査する設計とする。実際のWindows側呼出し、PnPとSSDTの名前解決・メモリ寿命・同一物理カウンタの動作は未検証であり、このリポジトリにはWindows受信コードを追加していない。`BQHR` はI2Cに触れない。AMLテンプレートは `Acpi/BqHandoff.asl` から生成し、`Tests/BqHandoff/test_bq_acpi_transport.py` は元ASLとの一致、アドレス置換、checksumに加え、ACPICAで電池SSDTを先に読み込んだときに128バイトBufferが返ることと、逆順では名前解決が失敗することを検査する。先行公開を確認できない場合はこのSSDTを登録しない。`Tests/BqHandoff/test_bq_dxe_host.py` はDXE実コードの停止状態、タイマー、EBS、初期化失敗のクリーンアップをホスト模擬する。

DXEはEBSと10秒タイマーを登録する。BQのPIOとread/writeコールバックのコードはあるが、条件証明関数が常にfalseのため登録されず、所有者は停止する。EBSでは状態2の非採用記録をNVSへ書き、キャッシュ書戻しを行う。記録は現在停止証拠であって、BQの最後の給餌時刻や現在のレジスタを取得したという意味ではない。Windowsドライバのゲートはそのまま閉じており、このFDの使用はBQの充電改善や継続保守を意味しない。EBS前に記録を読んだ場合はゼロで、Windowsの採用条件を満たさない。EBS失敗後の再試行でも停止は解除しない。

この構成では未解決のD0外・障害時維持を前提条件として扱い、BQアクセスを開始できない。将来実際の所有者を接続するには、I2C2排他、LK/UEFI間の期限、物理カウンタ、受信側、スリープ・停止・障害時の継続責任を両端で確立し、別途実機で検証する必要がある。

## タイマーからの実アクセスに向けた準備

`BqPio.c` は既存Windows MT8167 PIO実装を機種UEFI向けに移植した。readは0x6bのレジスタ指定とrepeated-start、writeはregister 00のTMR_RSTと既存SUPPLY_SEL以外を拒否し、busy/NACK/FIFO不一致/10ms・poll上限でバス障害をラッチする。タイマー・EBS・開始をTPL_NOTIFYで直列化し、初回アクセス前にコントローラ、SDA/SCL、EN、入力端子の状態を確認する。失敗・開始後のBeforeExitでI2C2を初期化し直さない。物理カウンタは成功した給餌より前の値を記録する。実際のMMIO・タイミング・他CPUとの排他は未検証。

`BqAdmissionVerified()` は常にfalseであり、設定やコンパイルフラグで解除できない。先行所有者の最終給餌に基づく絶対期限、Windowsが同一起動の記録を時間内に受け取る保証、全I2C2利用者の単一所有、D0外・OS停止・電源断・共有バス障害時に30秒watchdogを維持する実証済み主体または安全な引継ぎが揃わないためである。条件が未確立ならNVSへ停止記録を出すのみで、読み取りを含むBQの通信は起こさない。許可フラグ、テンプレートrecord、バッテリー残量やTOPRGU watchdogを条件成立の代わりにしない。

ホスト試験は実際の所有者とPIOを模擬バスに接続し、正常な一周期・recordのWindows側採用、途中NACKによる停止と非採用を検査する。実DXEの模擬では条件証明がfalseの場合、BQ MMIOの読書きが0回であることも検査する。これは実機での給電・watchdog継続を保証しない。
