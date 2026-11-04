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
