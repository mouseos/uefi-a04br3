# ARM32 MMU移植元

`Arm/` と機種パッケージの `Include/Chipset/ArmV7*.h` は、既存試験の
EDK II `e7aac7fc137e247edad22f7ee53b9a1fba227397`、`ArmPkg` のARM32実装を
コピーしたもの。BSD-2-Clause-Patentの原著作権表示を保持する。
共通EDK II/Mu_Basecoreを変更せず、機種用INFで参照する。

ソースとの差分は `ArmMmuLibCore.c` の非共有メモリPCD名を機種用
`PcdA04WinNormalNonshareable` に変更した1箇所と改行のLF正規化。既定FALSEは移植元と同じ。
正本の現在のMu_BasecoreにはARM32用MMUがなく、他の共通ライブラリにも
ARM実装の欠落がある。これはビルド、ページ表の動作、実機の証明ではない。
