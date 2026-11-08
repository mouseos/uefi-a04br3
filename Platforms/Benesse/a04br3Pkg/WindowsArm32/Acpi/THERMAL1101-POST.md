# MT8167 thermal parent1101 / RTC1100 / POST表示

基点e3b18c0ff1e35dfae54f00c54b6ae2203313c4d3のDIS0 framebuffer予約を保持し、7c183707の機種限定thermal PIO stop producer・今回起動LK校正・正規8rangeだけを選択mergeした。共通EDK/Silicon/BootShim非変更。

既存GPU_THERMAL_PROFILE=1とTHERMAL_PIO_HANDOFF=1を明示指定した場合だけ8range/停止producerが有効。RTC1100はWindows親側の別GUIDであり、このfirmwareへ新しいRTC time実装は追加しない。既存機種RTC library・GPU clock/IRQ/SD/CPU4/displayを変更しない。

Windows側はparent1101+thermal0.0.1.7、RTC0.0.1.0 frozen binariesと組み合わせる。旧1022親や6range bootへ混ぜない。Windows親は今回bootの校正v1/16byteと停止v2/72byteを検査し、未知状態は失敗する。一回のPIO温度取得候補でありcontinuous thermal保護・精度・強制remove/suspendの保持を実機確認済みとは扱わない。

105x HFP/AFE/SCO ABI変更はこのtupleに含まない。現親1026のAudio v11/capture v1がWindows側の基点。
