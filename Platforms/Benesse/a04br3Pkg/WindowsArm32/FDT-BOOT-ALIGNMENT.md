# 1c9d7ebe起動停止と早期FDT読取り

2026-11-05のrun1では候補bootのfastboot書込み・rebootはコマンド成功。
UART開始オフセット1127944382以降、初回起動はBOOT_REASON 4、LKは
`pc=0x40080000, r0=0x4e000000`をATFへ渡し、EL3 exit後に候補側の可読な
UART出力はない。次の起動以降はBOOT_REASON 9。次bootで収集されたCPU0
LASTPCは `0x430021dc` / `0x4300faa0` で、EL3側を指すが、それだけで
例外の種類や発生地点は決められない。fastboot自動復帰は観測されていない。
run1のstateは復元未実施で、実機操作・復元の有無をこの作業から推測しない。

該当boot.imgのMTK kernel内gzipを展開した結果は、記録された正本FD結合
成果物とバイト一致。BootShim内のFDT読み取りヘルパーは `ldr x0, [x0]`
という64-bitアクセスにコンパイルされていた。FDT property値の整列は4-byteで、
入力DTBの複数の `reg` 値は8-byte境界から4-byteずれている。LK時点の
SCTLR_EL2アラインメント条件は未観測のため、これを今回の停止原因と断定しない。
機種側の読み取りを明示的なbyte loadにし、guardと早期復旧のAArch64コンパイルに
`-mstrict-align` を設定する。リンク後の命令をホストで確認する。

`Tests/check_fdt_guard_aarch64.py` はQEMU Cortex-A53でEL2のSCTLR.Aを有効にし、
4-byte整列の `reg` を含むFDTを実際の検証器へ渡す。同条件で `1c9d7ebe` の
元コンパイルは例外handlerへ入り、修正版は正常に戻る。これはアラインメント
有効時の再現であり、実機EL2のSCTLR状態や今回の例外原因の観測ではない。

有限診断BootShimはUARTへ `~0`（入口）、`~1`（cache処理後）、`~2`（FDT検査前）、
`~3`（FDT検査return後）、`~4`（FDコピー後・ARM32遷移前）をそれぞれ送る。
UARTはDLABを確認し、1文字あたり有限回のみ送信準備を待つ。UARTが利用不能なら
出力を省くので、無出力や番号だけで次段の成功は主張できない。FDT検査失敗の
`A04FDT FAIL=0xNN` とRTC復旧状態出力は従来どおり。実機での動作・復旧は未検証。

## 実機のreserved-memory失敗の詳細

`b1c35271` の実機診断は `~0`〜`~3`、`A04FDT FAIL=0x06`、RTC watchdog武装、fastboot復帰を観測し、既知bootの書戻しコマンドが成功した。失敗６は５種類の分岐をまとめており、原因を区別できない。次の診断では拒否条件を維持したまま重複reserved-memory=0x0a、子にregなし=0x0b、非空ranges=0x0c、reg書式=0x0d、予約範囲の不正または衝突=0x0eに分ける。候補DTSにはregを持たない動的予約consysノードがあるが、実機のFDTと同一とは未確認である。

`a5308050` の実機UARTは `A04FDT RESERVED_NODE=consys-reserve-memory` と出力し、watchdogからfastbootへ復帰、既知boot書戻しコマンド成功。機種DTSはこのノードをsize/alignment各2 MiBの動的予約として記述する。次のguardは固定の物理アドレスがまだ指定されていないこの既知形式だけを受理し、それ以外のregなしやalloc-ranges付きは従来どおり拒否する。動的予約の物理領域を確保したり、後続OSでの割当てを保証するものではない。実機FDTのsize/alignmentが一致しない場合も拒否する。

## CPU・GICの循環依存

`df465ebd` 実機はFDT guard・FDコピーを通過してARM32 MMUとDXE Coreに進み、その後、破損したUARTにIRQ exceptionが記録された。fastboot復帰は240秒以内に観測できず既知bootは書き戻していない。端末固有CpuDxe.infはHardwareInterruptプロトコル待ちだが、使用中のArmGicDxe.infはCpuArchプロトコル待ちであり、両者のDXE依存関係が循環している。共通コードを変更せず、端末固有CpuDxeのDepexを上流ArmPkg CpuDxeと同じTRUEへ修正する。IRQ例外の原因と修正効果は実機未検証。
