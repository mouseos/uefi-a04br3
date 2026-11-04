# ARM32互換ソースの出自と未完成の接続

旧EDK II `e7aac7fc137e247edad22f7ee53b9a1fba227397` から、
同期ライブラリ、ArmExceptionLib、例外ハンドラー、ARM逆アセンブラ、
CpuDxe、ArmLib、HVC/SMC、CpuLib、BaseLib、IoLib、ProcessorBind.h、
ArmLib.hとAsmMacroIoLib.hの必要部分を機種固有ツリーへ移植した。
BSD-2-Clause-Patentの著作権・ライセンス表示を保持する。LF改行への
正規化、機種側ArmLibヘッダー参照、機種固有PCDへの名前変更、現在の
BaseLib.hに合わせた非アライン関数の引数型変更、INFのARM専用化を行った。
CpuDxeから未使用PCDを除外し、既存機種のGIC-400がGICv2であることに
基づくGICv5否定をArmLibに追加した。コードは正本の共通EDK IIや
MediaTekのファイルを編集していない。

Mu_Basecore `3c9c7b2e211e62d2a954ad760a36fe05adc43aa3` のPE/COFF共通
処理とSafeIntの32bit処理は機種側へコピーし、旧EDK IIのARM用relocationを
組み合わせた。構造体コピーでコンパイラが出力するmemcpyを機種側で提供する。
AArch64専用のloader patcherとSMBIOS生成コード、ARM未対応のcapsule
runtimeは試作構成から除外した。機能不足であり、代替の実装ではない。

試験では一時tools_defにARM用のProcessorBindとUefiBaseType、libgcc、
`liblto-arm.a`、BaseToolsコマンドへのパスを指定した。一時設定と古い
ビルド済みツールに依存しており、再現可能なビルドにはなっていない。
警告に出るlibgccのwchar・enum ABI差の影響は未検証。

重要: 現行Muの `ArmSetMemoryAttributes` は4引数（属性マスク付き）、
旧ARM32実装は3引数である。両者の扱いを一致させ、マスクで指定されない
属性の保持を検証するまで接続しない。今回のビルドはこの型の不一致で停止。
実機投入、FDの配布、DISMに使用しない。ページ表・例外・同期・PE relocation・
キャッシュ可視性もコンパイルと実機動作を確認できていない。
