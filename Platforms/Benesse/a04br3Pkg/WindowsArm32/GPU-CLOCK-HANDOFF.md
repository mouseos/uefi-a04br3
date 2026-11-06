# MT8167 GPU clock handoff

`A04BR3_GPU_HANDOFF=1` に加え、既定OFFの `A04BR3_GPU_MMPLL=1` を指定すると、既存MMPLLとMT6392 VCOREの状態を検証してparent7を選択する。ビルドスクリプトでは `A04_DIAGNOSTIC_GPU_MMPLL=1` を使用する。OFF、不適合、読出し失敗、選択失敗、状態変化、変数公開失敗はparent4（26,000,000 Hz）へ戻す。PLL・PMICレジスタ値は変更しない。PWRAPのbounded readはTouchPowerDxeと共用する。

MMPLLはAPMIXED `0x10018000` のcon0 `+0x160`、con1 `+0x164`、power `+0x170`、VCOREはMT6392 control `0x030a`、status `0x030e`、sel0 `0x0312`、sel1 `0x0314` を読む。周波数算出・電圧判定はUniXNU-a04br3の `a04_rgx_mmpll_hz` と同じで、253.5–390 MHz、1.15–1.30 Vの既存動作点のみを許可する。

成功時だけUEFI変数 `L"Mt8167GpuClock"`、GUID `9c50bc94-6c80-4fa2-939d-9278d07f7010` を公開する。attributesは厳密に6（BS|RT、NON_VOLATILEなし）。全フィールドはUINT32、little endian、計44 bytes、次の順序に固定する。

| offset | field | value |
|---:|---|---|
| 0 | version | 1 |
| 4 | bytes | 44 |
| 8 | parent | 7 |
| 12 | clock_hz | 検証済みMMPLL周波数 |
| 16 | con0 | 生値 |
| 20 | con1 | 生値 |
| 24 | power | 生値 |
| 28 | rail_control | 16-bit生値をゼロ拡張 |
| 32 | rail_status | 16-bit生値をゼロ拡張 |
| 36 | sel0 | 16-bit生値をゼロ拡張 |
| 40 | sel1 | 16-bit生値をゼロ拡張 |

起動時に既存変数を削除し、OFF時は公開しない。変数不在はparent4/26MHz互換。parent7のpower-on完了、実際のmux経路、選択前後のPLL/rail snapshot一致を確認してから公開する。runtime変数は状態の観測値であり、将来のクロック所有権を保証しない。Windows側はstage時とstart直前に実レジスタを再読出しし、recordおよびGPST parentと突合する（対応Windowsコミット `628998f`、ユーザー報告によるpush/build成功）。

ホスト試験: `TMPDIR=<作業ルート内の一時ディレクトリ> python3 Platforms/Benesse/a04br3Pkg/Tests/check_gpu_clock.py`。実コードのcallbackを模擬MMIO・PWRAP・変数サービス上で実行し、ON/OFF、ABI、電圧範囲、失敗時復帰を確認する。実機paired試験は別工程。

## 一度だけの診断出力

BeforeExitBootServices callbackの既存Attemptedガードを維持し、処理終端でbefore snapshot、実施した場合のみafter snapshot、outcomeを各1回DEBUG_ERRORへ出す。bounded poll内では出力しない。選択・公開前にUART出力を追加しない。

snapshotはcon0/con1/power/rail_control/rail_status/sel0/sel1、算出Hz、WACS、成功したrail読出しのbitmask、失敗PMIC addressとEFI_STATUS、PLL再読出し値を含む。rail_maskのbit0..3はcontrol/status/sel0/sel1に対応し、maskが0の値は未取得であり実測0を意味しない。PWRAP未準備でもPLL生値は記録する。

outcomeにはstageとreason、parent7 power-on result、publish status、parent4 fallback resultを残す。reasonはflag-off / pwrap-not-ready / pwrap-read / pll-disabled / pll-power / rail-status / rail-voltage / pll-rate / pll-unstable / power-on / clock-route / snapshot-changed / publish / accepted。parent4-requestedはfallback要求を意味し、fallback自体の成功はfallback_resultと区別する。record ABI・許可判定・PMIC/PLL設定書込み禁止は維持。

2026-11-07のユーザー指示により、この診断追加後のbootは生成しない。run18/run19で使用するb737a0f1の成果物を固定する。診断追加はホスト試験とcommit/pushまで。
