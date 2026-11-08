# Thermal producerの書込完了barrier

125f/8eを基点とする機種専用差分。run123でstage1/phase1(assert) STA0は実観測済みだが、以下はroot原因/解消の実機証明ではない。

## 比較した実source

CT3 Linux固定7c478212:

- mt8167/include/mach/mtk_thermal.hのTHERMAL_WRAP_WR32はmt_reg_sync_writel。
- mt-plat/sync_write.hは__raw_writel後にmb()。16bit版も__raw_writew後にmb()。
- arch/arm64/include/asm/barrier.hのmb()はdsb(sy)。機種defconfigはarch/arm64配下。

8eの実build mapは機種WindowsArm32/Library/BaseIoLibIntrinsicのIoLibNoIo.objを解決し、RegisterFilterLibNull.objを選択する。

- IoLibNoIo.cのMmioWrite32/MmioWrite16はvolatile storeとfilter呼出しだけ。
- null filterのBeforeWriteはTRUE、AfterWriteはno-opで、DSBを呼ばない。
- 機種BaseLib/Arm/MemoryFence.Sの実装はdmb→bx lr。これはcompletion用DSBではなく、当IoLibのwriteからMemoryFence自体も呼ばない。
- 機種ArmLib/Arm/ArmV7Support.Sに既存ArmDataSynchronizationBarrier=dsb→bx lrがある。今回それをそのままlinkし、共通/既存library sourceは変更しない。

したがってLinuxと同等の書込完了barrierがproducerの現write経路に無いことはsource上確定する。ただしbus/SoC側reset statusやgated clock動作がこの差で説明されるかは別で、STA0が解消すると断定しない。

## 差分と保持

機種producer内のlocal ThermalSyncWrite32/16は既存MmioWrite32/16直後に既存ArmDataSynchronizationBarrierを1回呼ぶ。既存の32bit7箇所/16bit2箇所の全writeに適用する。module INFへArmPkg/ArmLib依存を追加するだけで、共通EDK/機種library自体は編集しない。

既存write/readのアドレス/順序/個数/width/value/maskとstrictSTA/ready/record条件を維持する。extra MMIO access、delay/retry、clock順変更、gate緩和、reset成功合成、未知writeは無い。全8range/ACPI/4CPU/UART/自動復帰/resetは既存profileのまま。

tests/thermal-write-completion.pyは同じ境界MMIO model上のproducerを実行して、barrierイベント以外のtrace/72byte record/3failure+passを比較する。全実行write直後に1barrier呼出しを確認するが、host stubは実DSBやMMIOを実行せず、hardware成功試験ではない。実ARM buildのlibrary選択はlink map/sourceで別確認する。

次の通常一括にmain/Menciusが選択同梱するboot候補。今回実機・専用boot・ready0解消の主張は無い。親1104のresource統合候補とは独立し、Windows SYS/署名や媒体は変更しない。
