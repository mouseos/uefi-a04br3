# 4 CPUとwatchdog所有権

LinuxのMT8167はPSCIを使って各CPUを起動し、watchdogは別のOSドライバが
所有する。XNUにも4 CPU稼働と多数のIPI送受信の実機記録があるが、どちらも
Windowsでのparking protocolやwatchdog引継ぎの証明にはならない。

過去の3 CPU WinPE実績ではCPU1はUEFIのworkerに残り、WindowsはCPU0/2/3を
使用した。別の4 CPU実験ではCPU1をWindowsへ渡し、TOPRGUの早期所有ドライバが
起動しないままExitBootServices後にwatchdog再起動、次bootはBOOT_REASON 9。
`windows-rt-mma8652-integrated-run2/first-boot-uart.log` に記録される。
対して早期TOPRGUドライバを含む別の4 CPU WinPE試験では
`A04RGU CPU1 ownership acquired CPUs>=4` と周期kick、GPU copyの完了が記録され、
別の試験ではMMA8652の静止時1サンプルも得た。これらは旧edk2-mtk構成での
観測であり、製品版Windowsや長時間動作を証明しない。

後続の正本UEFI `1b705d65` と監査WinPEの組合せでは、同一起動のUARTで
`A04RGU CPU1 ownership acquired CPUs>=4` と継続給餌を確認した。ただし
搭載Windowsによる候補SYSの署名受理、4 CPU動作や長時間安定は未確認。
本変更を含むファームウェアはビルド・実機検証していない。

WindowsはCPU1を要求しない限りUEFI workerが給餌を続け、CPU1への有効なparking
要求が届くとworkerが最後のkickを行いCPU1をWindowsへ移す。OS側ドライバは論理
CPU1上のDPCでMPIDR affinity 1を確認後にTOPRGU所有を主張する。未導入・署名拒否・
PnP開始遅延の場合はworker退場から31秒以内に新所有者が給餌できず、watchdogで
再起動し得る。boot-start指定やファイル配置のみでは31秒以内のD0到達を保証しない。

機種BootShimの4 CPU試作は、CPU1移譲直前にTOPRGU MODEのenable bitを読戻す。
無効ならCPU1を渡さず、一度だけ `~6` を試行出力し、既存workerの期限まで残る。
有効なら `~5` を試行出力し、最後のkickの後でparkingへ移る。これらはUARTが
送信可能な場合だけの診断であり、Windowsの所有成立・継続給餌の証拠ではない。
後者はWindowsの `A04RGU CPU1 ownership acquired CPUs>=4` とkickを別途確認する。
MODEが無効のままの場合、worker期限後の自動復帰は保証できない。読戻しと移譲の
間・移譲後に他主体がMODEを変更する競合も完全には防げない。

この4 CPU構成はDISMなどのディスク更新に使わない。署名が受理される早期TOPRGU
ドライバと対応WinPE媒体、外部reset主体の排他、復旧手段を主担当が用意し、
実機UARTと次bootのBOOT_REASONを観測するまでは未検証。RTC・電源・UART・watchdog
障害時の復帰も保証しない。3 CPU構成やGPU既存動作の成功と混同しない。

## CPU1のL1保守実験

CPU2・CPU3はparkingに入る前にcacheのclean/invalidateを実行する一方、CPU1の
watchdog workerからの引継ぎは命令キャッシュをinvalidateするだけで、データ側の
状態を明示していなかった。CPU1で実際にdirtyなL1が存在したことや、これがWindows
不安定の原因であることは未観測。実験フラグ `A04BR3_FOUR_CPU_L1_CLEAN=1` は、
CPU1が有効なmailboxと有効なwatchdogを確認し最後のkickをした後、AArch32へ
切り替える前にCPU1のL1データ／統合cacheだけをset/wayでclean・invalidateする。
他のCPUが既にWindowsで動作し得るため、共有L2のset/way保守は行わない。
CLIDRでL1データcacheの存在を確認し、CCSIDRの従来形式とCCIDX形式を扱う。
ビルドでは `A04_DIAGNOSTIC_FOUR_CPU_L1_CLEAN=1` を使用し、4 CPU診断時だけ
有効にできる。既定値は0。ホストの命令エミュレーションは状態遷移と演算を
検査するが、実機cacheの同一性・共有性、OSとのcoherency、実行時間、停止・復帰や
長時間の4 CPU安定を保証しない。実機試験は主担当が別途行う。
