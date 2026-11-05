# Windows ARM32 BQHR 停止記録

`mouseos/edk2-mtk` の `691f907c6835bcc9992479c29e6aed0eba2f116c` と後続 `e8af0fb0a2cdf2b592ad16d63b4ad947a993a94b` 由来の機種固有実装。既定OFFの `A04BR3_BQ_HANDOFF=1` と `A04BR3_BATTERY_PROVIDER=1` で通常のI2C2引継ぎドライバに代えて選択する。別のcharger ownerとの同時利用は拒否する。ACPIのBAT0公開を確認し、NVSへ停止記録を置く。BQへの通信は所有証明がないため開始しない。

この正本のWindows ARM32ビルド・実機での受理、ACPI順序、引継ぎと充電は未確認。原版の停止記録の過去の実測は、この版の成功を意味しない。firmwareとWindowsのI2C2排他、電源、watchdog継続は別途検証する。EDK2共通コードは変更していない。
