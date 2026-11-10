# LKパネルタイミング引継ぎ

機種専用機能で、既定は `A04BR3_PANEL_TIMING_HANDOFF=0`。既存統合表示ビルドで `A04_DIAGNOSTIC_PANEL_TIMING_HANDOFF=1` を指定して有効にする。EDK2共通ソースは変更しない。

SECはLK FDTの `/chosen` または `/chosen@0` にある `atag,videolfb` を検証する。CT3 kernelの `mt8167/videox/mtkfb.c` が定義するlittle-endianの配置は、u64 framebuffer、u32 found/fps/vram、NUL終端のパネル名。実パネル `auo_wxga_dsi` と実LK POSTアドレスを確認し、欠落・不正な値を60Hzなどで補完しない。FPSの単位は0.01Hz。kernelは `videolfb_tag->fps` を継承し、2フレーム待ちに `msleep(2*100000/lcd_fps)` を使い、6000を60Hzと記述する。

機種GUID HOBに起動時の値を保存する。DXEはReadyToBootで表示SSDTの存在を確認してから、別SSDTとして `\_SB.DIS0.DTM0` を公開する。MMIO書込み、資源の再割当て、DMA所有者や実周波数の変更はしない。timing欠落・不正時はDTM0を公開せず、利用側で明示的に失敗させる。DMS0と既存資源の互換性は維持する。

DTM0は64Bのlittle-endian DWORD列：`magic=0x314d5444,version=1,bytes=64,flags=1,panel_id=1,width=800,height=1280,total_width=960,total_height=1300,fps100=<実LK値>,denominator=100,reserved[5]=0`。total geometryはvendorパネルのh4/132/24・v4/8/8に由来する。利用側は実FPSとtotal geometryから整合する信号rationalを生成し、DSI PLL値をpixel rateへ直接代入しない。

`generate-aml.py` はPanelTiming.aslをiaslでコンパイルし、追跡対象PanelTimingAml.hと一意のrecord offsetを生成する。有効ビルド時に再生成一致を検査する。DXEはデータとchecksumを設定したテーブルをprotocolにコピーさせる。生成済みFDの手修正は行わない。

ホストparser検証（出力先はプロジェクト内）：

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -IPlatforms/Benesse/a04br3Pkg/Include Platforms/Benesse/a04br3Pkg/WindowsArm32/Drivers/PanelTimingDxe/test-parser.c -o "$PROJECT_LOGS/panel-parser"
"$PROJECT_LOGS/panel-parser"
```

parser／iasl検証、ARM firmwareビルド、実機動作は区別する。本機能は実機未反映・未検証。
