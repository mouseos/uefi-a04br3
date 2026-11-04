# ARM32診断入力の静的照合

追記: 後続の試作では機種BootShimへFDコピー前のFDT検査とRTC経由の有限時間
復旧を接続した。以下の未接続に関する記述は変更前の記録。実際のLK FDT、
動的予約の表現、復旧の実機動作は依然として未検証。

`scripts/audit-boot-inputs.py OTA.zip STOCK_BOOT DTB EDK2_MTK_REPO` は、OTAの
boot.imgとのバイト一致と機種・版metadata、DTBと `mouseos/edk2-mtk` の固定
`1982d888c765f5b12f6e0dce71f1ed8bc92ac50b` blobとの一致、Android header、
MTK ROOTFS長、入力DTBの固定予約を照合する。OTAの署名認証と実機LKの同一性は
この検査に含まない。

手元の02.00.000 OTAではboot SHA-256 `7ad5998e58eeecc5ba059d868c44186cc6557f6013b45876ee728f65f8c892b3`、
LK SHA-256 `3b40734d16491f162da76590cde92abc182da7791a7f0508d552da0d5568c5e1`。
DTB SHA-256は `07cef7be51fc3cbf73a19c5c5d26c5676d9bf85743287a2c195917138731a81c`。
stock headerはkernel `0x40080000`、ramdisk `0x44000000`、tags `0x4e000000`。
ROOTFSヘッダー除去後の推定initrdは `0x44000000..0x4420612d`。このLK挙動は
別バージョンのLKについての静的解析に基づく候補であり、このOTA LKや実機での
動作証明ではない。ARM32機種マップは `0x44000000..0x44400000` とDTBの固定
audio DMA `0x5c000000..0x5c400000` を予約する。

入力DTBの `/reserved-memory/consys-reserve-memory` は動的割当で、LKも実行時に
メモリ情報・atag・chosenを更新する。実際のFDT、メモリ予約、initrdの位置・長さ、
FDTの拡張範囲、および現在の端末のLKは未観測。現行BootShimは実行時FDTを検査する
前にFDへコピーし、PrePi入口は固定スタックを使う。したがって、これらの動的領域と
コピー先・スタック・HOBの非重複を保証できない。静的照合と再現FDをもって実機投入
可能と判定しない。早期の安全なFDT検証と実機証拠が必要。
