# ARM32 Windows RT 正本への移植

## 現在の作業ツリーの状態

コミット `6fdba49a` の機種ソースと、固定Mu `3c9c7b2e` 等のクリーン依存から、
`A04_DIAGNOSTIC_THREE_CPU=1` で同じ出力パスに2回ビルドした。FD、BootShim、
結合FDはそれぞれバイト一致し、入力・ツール・設定のハッシュをmanifestへ
記録した。診断専用であり、実機用の完成品ではない。結果はビルド先の絶対
パスを含むため、別のパスでのバイト一致は主張しない。

リンク警告を出すlibgccの7個の整数ヘルパーを、実際のARM GCCライブラリに
リンクした `Tests/check_arm_libgcc_runtime.c` でqemu-arm上の代表値について
実行した。整数の代表値は通過したが、ELFのwchar_tとenumの属性不一致は
残っている。テストは警告の解消、完全なABI互換性、UEFI上での実行、
正本構成の実機動作を証明しない。

追加の非採用試験として、Ubuntuの `gcc-arm-none-eabi_13.2.rel1.orig.tar.bz2`
（SHA-256 `2db06abe865ce68eced25e449aecb45cdb6c151c3551c739620cfc3730214c91`）
から `libgcc2.c` とARMの `bpabi-lib.h` を使い、7関数をshort wchar・32-bit enumで
コンパイルした。独自に空の生成用ヘッダー等を仮置きした実験であり、正式な
GCC構成とは未照合。代表値のqemu-arm試験とリンク警告なしを確認したが、
再現可能な依存ビルド・広い入力範囲の検証・ファームウェアへの接続は未完了。
この試作オブジェクトはFDへ組み込んでいない。

その後、再生成した7オブジェクトについて、元のlibgccと配置対象の全セクション、
公開シンボル、再配置、およびwchar・enum以外のARM属性が一致することを検査する
診断用overlayビルダーを追加した。wcharは2バイト、enumはintとして再コンパイル
している。旧・新の両ヘルパーで4096組のARM qemu試験が通り、overlayを使用した
3 CPU診断FDの2回のビルドは一致し、対象ABI警告も出なかった。最小の生成用
ヘッダーは正式なGCC生成物ではなく、ターゲット上のABI・動作は未検証である。

ホスト側の読取り監査では `a04br3-ce66352-recovery.service` がinactiveだが
enabledで、後から開始され得る状態だった。UART受信を除外したworkerだけでは、
このサービス、別プロセス、別ホスト、ハードウェアからのリセットを排除できない。
DISM用runnerの停止条件は解除しない。CPU1のfeedカウンタもwatchdog自体の
有効化やOS引継ぎ後の継続を証明しない。

`A04BR3_WORKER_NO_UART_RESET=1` の試作は無期限workerのUARTリセット受信と
UART診断出力を除外し、給餌レジスタへの書込み後に共有ページのカウンタを
増やす。対応するDXE設定 `A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT=1` は
カウンタの2回の変化を観測できなければ停止し、TOPRGU modeを記録する。
このカウンタはwatchdogの有効化、継続動作、CPU故障時の保護、OS側やホスト側
など他主体のリセット排他を保証しない。BootShimとDXEは必ず同じ試作設定を
用いる必要がある。正本構成で実機検証しておらず、ディスク保守は禁止。

以下の各節は当時の履歴であり、未接続やビルド失敗の記述を現在の状態と
混同しない。専用worktreeではPrePi、ACPI、I2C1をDSC/FDFに接続し、
ARM32診断FDを生成した。旧 `b8ca32e` を出自とする3 CPU DXEも試作し、
CPU1の応答とCPU2/3のparkingを確認した後でSSDTとMADTを公開する設計を
追加した。`A04BR3_EXPERIMENTAL_THREE_CPU` の既定値は0で、1は試作専用。
`A04BR3_DISK_SERVICING=1` との組合せはビルド時に拒否する。

CPU1の初回マーカーはwatchdog有効化や継続的な給餌の証拠ではない。workerの
UARTリセット受信、外部監視によるリセット、watchdogの実際の状態と所有権、
BootShimとの接続、OS引継ぎ後の動作はいずれも保守用途の安全性が未検証。
ARMのlibgccにはwchar_tとenumのABI属性警告が残る。3 CPU試作FDは実機用の
完成品ではなく、書込みやDISMに使わない。旧構成でのWinPE実績はこの
正本構成での実機動作の証明にならない。

ホストmockでは実際のARM32ページ表更新コードの属性保持と未マップ範囲の
事前拒否、I2C NVS公開のflush順序を検査した。これは実機のページ表配置、
TLB・キャッシュ可視性、NVS保持の検証ではない。SSDTの生成元ASLと埋込み
バイト列の一致は同じiaslバージョンで `Tests/check_windows_cpu_ssdt.py` により
検査できる。

このディレクトリは `mouseos/edk2-mtk` の機種専用 `A04BR3Pkg`、
コミット `b8ca32e` からの移植途中のソースである。BootShim、PrePi、
単一CPUのACPI基盤を保存し、元の著作権・ライセンス表示を保持する。
当時のWinPEでの実測を正本構成の動作確認として扱わない。

BootShimをAArch64アセンブラでコンパイルし、DSDTをiaslでコンパイルした。
正本のDSC/FDFには登録していない。PrePiは元のMediaTekPkg・A04BR3Pkgの
PCD、ライブラリ、GCC用エントリとMMUラッパーに依存し、正本のMu系ライブラリ
にそのまま接続できる保証はない。ACPIのインストールとI2C1 DXEも未登録。
現在のDSDT/MADTは単一CPU構成で、既存の3 CPU実測構成ではない。

正本のARM32ビルド・ACPIとI2C1の接続・watchdog停止・Windows起動・
署名受理・実機検証は未完了。これらのファイルや単体テストの成功を
ディスク保守に使用できるFDとみなさない。共通EDK2、Mu_Basecore、
MediaTek共通コードは変更しない。

`Acpi/CpuParkingFourSsdt.asl` は旧 `b8ca32e` のCPU1〜3の名前空間を
移植した候補であり、DSDTとのUID 0〜3をacpiexecで検査した。単体では
4 CPUを起動しない。旧実装はDXEが各CPUの予約parking pageを確保し、
CPU2/3起動とmailboxを確認したうえでMADTを差し替え、CPU1は既存の
watchdog workerがparkingに移行する。4 CPU時のworker解放後はOS側の
watchdog給餌を前提とする。旧 `71eb45b` の無期限給餌は3 CPU限定で、
4 CPUと組み合わせないよう明示的に拒否されている。

よって4 CPUの名前空間のみを公開してはならない。正本のメモリマップ・
PSCIとparking pageの所有権、初期化失敗時のACPI整合性、watchdogを
有効にしない場合の全CPUの起動・復帰経路を設計し、ホスト検証と実機
証拠を得るまで、DSC/FDFへ4 CPUまたはI2C1を有効化しない。

## 3 CPU保守workerの部分移植

BootShimに既定0の `A04BR3_OFFLINE_WDT_UNBOUNDED` を追加。1のときは
CPU1の給餌期限比較を省く。アセンブラのコンパイル時に、3 CPU SMP、
A64 worker、CPU1専用所有を要求し、4 CPUとwatchdog引継ぎ機能を拒否する。
有限版と無期限版の実際の命令、無効な組合せの拒否はホストで確認した。
CPU1はリセットUART受信も行うため、外部のリセット要求を防げない。
ハードウェア故障、counter周波数ゼロ、別主体によるwatchdog変更、電源喪失、
実機上の給餌・WinPEの動作をこの検査は保証しない。

`CpuParkingSsdt.asl` は旧実績構成のCPU2とCPU3を宣言し、CPU0と合わせ
UID 0,2,3になることをacpiexecで検査した。MADTの動的置換、parking page、
CPU1 worker起動とその有効性の確認、およびI2C1のDXE登録は未接続。
PrePiが参照する旧 `PlatformPrePiLib`、`PcdUefiMemPoolBase/Size`、
`A04BR3Pkg.dec` は現行正本の同名実装が存在せず、ライブラリとPCDを
機種固有に設計してからMuのDSC/FDFへ接続する必要がある。未接続状態では
正本から3 CPU WinPE FDを作成できない。DISMや実機書込みに使わない。

## PrePiのSEC依存の接続準備

元の `PlatformPrePiLib` は正本にはなく、正本のMT8167 SEC初期化は
`PlatformSecLib` とそのboard hookを使う。PrePiのINFとincludeを正本の
パッケージ名・ライブラリ名へ修正し、watchdogの操作をSECのboard hookに
集約した。旧PrePiにあった追加のwatchdog武装は削除した。SEC初期化を
PrePiMainより先に一回呼ぶことは静的に確認したが、コンパイルも実機も
未検証。旧Windows専用CPU 1300 MHz切替経路は接続していない。

旧UEFIプールは `0x44550000..0x4BAFFFFF`、旧初期スタックは
`0x44500000..0x4453FFFF`。正本のスタックは `0x4E200000..0x4E23FFFF`、
DXE heapは `0x4E240000..0x51FFFFFF` であり、旧PCDをそのまま流用できない。
PrePiのHOB作成が算出するスタック位置と実際のSP、ファームウェアFV、
ページテーブル、ACPI NVS、RAM mapを一緒に設計・検証してからPCDを
接続する必要がある。旧サイズ・アドレスを未検証のまま正本に設定しない。

## 候補メモリ配置

Windows ARM32専用候補 `Arm32Memory.dsc.inc` と専用MemoryMapLibは、FVを
既存正本と同じ `0x41080000`、HOBとDXE用領域を `0x4E240000..0x51FBFFFF`、
スタックを `0x51FC0000..0x51FFFFFF` に置く。旧AArch64専用スタック領域は
Windows候補マップでは通常メモリへ戻す。HOBアリーナの末尾に実スタックが
一致することをPrePiが検証し、不一致時には停止する。MMUページ表は固定
アドレスで予約せず、MemoryPeimが初期HOBから割り当てる前提である。
I2C1のNVSはDXEで `EfiACPIMemoryNVS` ページを動的割当てし、固定アドレスを
仮定しない。I2C1のMMIO範囲も専用マップに追加した。

ホストの範囲検査はスタック・HOB、現在の固定予約領域との重なりを検査する。
実際のページ表割当・HOB解釈・NVS保持・キャッシュ整合性を検証しない。
候補のDSC断片はまだ本体DSCに読み込まれず、SEC/ACPIのFDFへの接続と
再現可能なARM32 FDビルドも未完了。ディスク保守には使用しない。

## DSC/FDF接続の試作と正本依存ビルド

`WindowsArm32.dsc` / `WindowsArm32.fdf` はPrePi、ACPIテーブル、ACPI table
protocol、I2C1 DXEの候補接続である。既存AArch64設定とは別ファイルで、
UniXNUの起動ドライバとDT公開を取り除いた。ただし現時点では単一CPU MADT、
CPU worker起動・parkingの欠落、表示や記憶領域引継ぎの未検証があり、
実機投入や保守用途には使えない。

正本Mu_BasecoreのBaseToolsと既存依存キャッシュを参照してARM/GCC5ビルドを
試した。旧PrePiが要求する `LzmaDecompressLib` は正本MuではNULLクラスの
コンストラクタなのでPrePiの個別ライブラリ指定へ変更した。その後、共通
`SiliciumPkg/Sec/Sec.inf` が参照する `PcdPrePiCpuIoSize` がMu_Basecoreの
`EmbeddedPkg.dec` でARM向けに宣言されていないため、メタデータ検査が
エラー3000で停止した。PrePi自身も同じPCDに依存する。共通コードを編集せず、
機種専用ライブラリや別構成でSECを置換し、他のMu側依存も検証する必要がある。

ページ表・ACPI NVSの実割当、NVSのキャッシュ可視性、ACPIインストールは
まだビルド・実機検証できていない。試行ログは作業環境の
`/tmp/a04-canonical-arm32-build3.log`。再現可能なFDは生成されていない。

## ARM向けPCDと共通ライブラリの追跡

元の `PcdPrePiCpuIoSize` はARMでは値0（I/Oポート空間なし）を使っていた。
機種用 `PcdA04WinCpuIoSize` の既定値0に置換。正本共通SECがARMでは存在しない
PCDを参照するため、機種専用DSC構成からそのSECだけを外し、同梱PrePiを使う。
共通DSCには手を加えず、機種用の構成断片を設けた。

次に現行Muの `ArmMmuBaseLib` がAArch64ソースのみであることが判明した。
旧EDK IIのARM32 MMUとヘッダーをライセンス表示・出自付きで機種側に移植し、
非共有メモリの既定FALSEを機種PCDで維持した。さらにARMを持たない
`ArmMonitorLib` を旧EDK IIのC版へ差し替えた。共通コードは変更していない。
これらの互換性・ページテーブル動作・保守時の安全性はまだ未検証。

ARM/GCC5の再試行は以前のPCDとMMUのメタデータ検査を越えた後、
`BaseSynchronizationLib` と `DxeCpuExceptionHandlerLib` にARMのソースが
ないため停止した。正本Muは他にもAArch64専用化されている可能性があり、
候補FDの完成や実機での安全性は主張できない。詳細ログ:
`port/gsi/logs/ios-feasibility/windows-rt-canonical-arm32-build8.log`。
