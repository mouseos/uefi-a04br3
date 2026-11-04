# ARM32 Windows RT 正本への移植

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
