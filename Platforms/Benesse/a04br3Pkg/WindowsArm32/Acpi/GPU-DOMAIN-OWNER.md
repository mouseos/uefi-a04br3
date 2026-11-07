# SD-onを維持したGPU domain owner ACPI

本branch `work/windows-gpu-domain-owner-sdon-53d6` は53d6b645を基点とする機種専用変更。
Carver thermalの別53d6 branch、canonical checkout、共通EDK2には変更しない。

A04BR3_GPU_DOMAIN_OWNER=1は既定OFFで、SHARED_IO_PROFILE=1とSD_PIO_PROFILE=1が必須。
共有ACPI producerはAcpiTablesSharedGpuDomain.infだけを選び、既存SD-only sourceの全tableに
SHR0/BEN0001の第六exclusive SPM resource (0x10006000/0x1000)を加える。
SdPio.asl、DisplayProviderのbrightness資源、GPU0 core/MFG/IRQ、GPSTを保持する。
新しいAML register writeは追加しない。FILE_GUIDは既存loader PCDと同じ
7E374E25-8E01-4FEE-87F2-390C23C606CD。full build時の既存storage validatorも保持。

対になる親1014は9e07f0a SD-on1010 ancestry＋全実flags/audioDMAforwarding＋GPU domain V2。
親1008/1010のまま第六resourceを追加したり、SPMのunassigned physical fallback mapはしない。
INFRA-only parentのlibrary成功は最終tupleではない。
親が書くのはMT8167 MFG(0x214)/MFG_2D(0x2c0)/MFG_ASYNC(0x2c4)とINFRAの固定GPU bitだけ。
PWRON_CONFIG_EN(0)は既存unlockのreadのみで、key/CPU4/watchdog/wakeup/TOP/PLL/PMICは書かない。

機種build helperはA04_DIAGNOSTIC_GPU_DOMAIN_OWNER=1を明示した時だけdefineを追加し、
effective define/resourceとsourceをdiagnostic-manifestへ記録する。既存SD/全baseline flagsも渡す。
ASL standalone compile、fullFD build、署名package、実機正常停止/描画は別々の検証段階。
本source変更は実機flash済みという意味ではない。
