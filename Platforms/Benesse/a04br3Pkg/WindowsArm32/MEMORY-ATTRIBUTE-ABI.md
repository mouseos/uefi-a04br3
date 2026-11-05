# ARM32メモリ属性のABI

2026-11-05の正本 `311b6f57` 実機起動では、MMU有効化前のUARTページ記述子は `0x1100543a`。ARMv7短形式ではTEX=0、C=1、B=0で通常のwrite-throughメモリを示す。端末memory mapではUARTをDeviceに指定している。

SiliciumのMemoryInitPeiLibとMemoryMapLibはMuの `MdePkg/Include/Library/ArmLib.h` を使用し、Device=6、write-back=1を渡す。一方、端末固有のARM32旧版MMUライブラリが参照する `A04ArmLib.h` は旧EDK II由来の並びで、6をwrite-through、1をnonsecure uncachedと解釈していた。2つの宣言は別の翻訳単位にあるためビルド時に検出されない。これは実機で記録したUARTページ記述子と一致するが、UART文字化けやIRQ例外の唯一の原因とまでは未確定。

共通EDK II・Mu・Siliciumは編集せず、端末固有互換ヘッダの共通属性の数値をMu側と一致させた。旧ARM32ライブラリ固有のnonsecure属性は別の値とし、NS判定も明示的な範囲に変更した。端末メモリマップの実際の属性はuncached、write-back、write-through、deviceに限られる。MuのRO/XP属性を旧ARM32ライブラリが正しく実装することは、この変更では保証しない。

`python3 Platforms/Benesse/a04br3Pkg/Tests/check_arm_memory_attribute_abi.py MU_BASECORE` は、使用するMuヘッダと端末用ヘッダの数値をコンパイル時に比較する。実機でUART/GICページの属性、MMU後の出力、DXE、復帰を確認するまで修正の実効性は未検証。
