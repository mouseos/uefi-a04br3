# MT8167 thermal stage1失敗readbackの観測

6c96c6f6基点。既存assert/ungate/deassertの各MmioRead32戻り値をlocalへ一度保存し、既存失敗条件が成立した場合だけDEBUG_ERRORへphaseとactualを出す。

- `A04 THM PIO stage1 phase=1(assert) actual=%08x`: INFRA+0x128の既存assert判定が拒否。
- `... phase=2(ungate) ...`: TOP+0x24の既存clock mask判定が拒否。
- `... phase=3(deassert) ...`: INFRA+0x128の既存deassert判定が拒否。

MMIO read/writeの回数・順序・値・失敗条件、volatile変数v2/72byte、stage/ready生成、時計・校正・親ABIは変更しない。追加MMIO、poll、delay、clock mask拡大、reset成功合成はない。readback値はgate/reset状態であり、校正rawは出力しない。既存最終summaryログも保持する。

次の通常GPU/周辺更新で機種packageだけを統合し、その同一bootのUARTで回収する。専用実験bootは追加しない。このsource変更から根因確定・停止保証・温度取得成功を主張しない。現6c96/1d248 artifactやmain source/媒体は変更しない。

`WindowsArm32/Tests/thermal-stage1-readback.py --output <root内output>` は実6c96/sourceと変更sourceを3失敗枝＋stage1通過枝で実行し、MMIO traceと公開72byte recordが完全一致、追加DEBUGが対応phase/actualだけであることを確認する。実MMIO試験ではない。ARM production TUは既存6c96生成headerと同一flagsで専用outputへcompile確認し、FD/bootは生成しない。次正規一括の統合担当がそこで固定sourceから通常buildする。
