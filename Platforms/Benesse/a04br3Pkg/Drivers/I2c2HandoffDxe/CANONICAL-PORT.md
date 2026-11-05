# Windows BQHR実装の正本への移植状態

この機種専用ソースは `https://github.com/mouseos/edk2-mtk` のWindows向け実装 `691f907c6835bcc9992479c29e6aed0eba2f116c` に由来する。`windows-rt/arm32` の後続 `e8af0fb0a2cdf2b592ad16d63b4ad947a993a94b` は同コミットを祖先に含み、給餌間隔の起点と時計逆行の検査を修正している。ここにはDXE、PIO、所有者、core、AMLとテストを `e8af0fb` から移した。元のMITライセンスと出自は各ソースおよび `Library/Bq24160HandoffLib/README.md` に残す。INFのパッケージ参照とテストのパスのみ、この正本の配置へ変更した。`Tests/BqHandoff/BatteryGauge.asl` は元のACPIテスト用fixtureであり、現在の正本にACPI batteryを公開するものではない。

`691f907` を基にした別系統の３ CPU Windows診断ではBQHR停止recordとLC709203F残量が観測された。正本のこのソースやイメージでの実機動作、BQ24160給餌、充電成功の証拠ではない。state 2とreason 6の停止recordは当該producerの状態を表すが、先行LKの通信、時計の同一性、Windows以外のCPUやデバイスのI2C2使用、給餌期限の証明ではない。FNV sealも認証ではない。

正本の現行board DSCはAArch64/FDTであり、Windows ARM32・BAT0 ACPIとACPI tableプロトコルがない。既存 `A04BR3_CHARGER_OWNER` は別のUniXNU向けI2C2所有者である。I2c2HandoffDxeとBQHRはDSC/FDFに登録しておらず、実行・BQ通信は発生しない。`A04BR3_WINDOWS_BQHR` は既定0で、1を指定してもビルド時に明示的に拒否する。これは完成した有効化フラグではない。ARM32/ACPI基盤、BAT0公開順、双方の排他、EBS/NVS寿命、watchdogとD0外・障害時の責任が実装・検証されるまで、二つの所有者を同時に有効にしない。

検証: `Tests/BqHandoff` の所有者・PIO・DXE・ACPICA試験はMMIO、時計、boot serviceを模擬する。結果はホスト限定で、実際のfirmware build・boot、電圧・給電、Windowsドライバとの同時実行を保証しない。実機操作は禁止した状態で移植した。
