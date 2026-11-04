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
