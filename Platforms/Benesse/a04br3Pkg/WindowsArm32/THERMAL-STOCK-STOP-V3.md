# CT3 software停止 handoff v3

CT3 7c478212 mtk_ts_cpu.cのtscpu_reset_thermalはSET/CLR syncwrite対、STA pollなし。probeはclock_on→reset、resumeはreset→clock_on。STA128定義はclockgate中assert latch資格の根拠ではない。v3はresume操作順を使い、clock有効下でIRQ0→AHB idle→pause10e→MON0→AHB idle、ADC11 auto解除/trigger0/busy0、最後の停止再読を必須にする。gated zeroはidle証拠にしない。最終reset再assertをせず、pauseを解除しない。新MMIO/未知write/ready合成なし。

72byte/attrs6/GUIDは維持、version3で意味を区別。reset_afterは観測だけ、v2 reset-heldの意味を密かに緩めない。対応Windows親がthermal/HP両端でlive pause10e/MON/IRQ/AHB/ADCを検査する必要がある。旧親はv3を拒否するのでboot単独採用不可。単一normal-world owner移管でありsecure firmware不干渉の証明ではない。

167b050e/bb753417のidle候補、現560cf/GPU135は不変。RTC/10ACPI/8range/4CPU/shim/復旧を保持。ホストモデル/ARM buildは実温度・HP校正・ready1の実機証拠ではない。
