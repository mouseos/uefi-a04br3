# Charger owner review candidate (default OFF)

`A04BR3_CHARGER_OWNER=1` includes the early DXE module after TimerDxe. Default0
excludes the module, so no charger/clock/GPIO access occurs. This is board-only;
no platform-wide MCU/PMIC/I2C changes or binary patches are prerequisites.

Pair with reviewed device `a04br3-charger-owner=1` before runtime. Do not ship or
flash just one side as a qualified charging fix. Main owns runtime deployment;
Chandra source/host review passed; target SDK/runtime qualification is pending. No charging improvement is measured.

The identical `Include/A04BR3ChargerCore.h` and `A04BR3ChargerBoardCore.h` copies
are explicitly synchronized from the device repo's `package` files. Verify:

```
python3 Platforms/Benesse/a04br3Pkg/Tests/sync_charger_core.py --device-root /absolute/UniXNU-a04br3
python3 Platforms/Benesse/a04br3Pkg/Tests/test_charger.py
```

Use `--write` only for deliberate device -> firmware synchronization, then review
both repos. Pin both commits for any deployment; matching filenames are not proof.

First record inherited state, then initialize I2C2's exclusive gate bit16/mux bit3,
SDA60/SCL61 ALT1/input-enable/bias-disable, and open-drain PIO timing <=100 kHz.
The AHB parent/MAINPLL is read, not reconfigured; unknown/off parent is an explicit
failure. No I2C0/PMIC/APDMA/shared parent-clock modification, charger enable drive,
controller reset, or arbitrary charge/current/voltage/safety configuration write.
BQ24160 policy must already match the observed LK settings and be fault-free.
Only register00 TMR_RST with unchanged SUPPLY_SEL is allowed as charger data write.
Stock source: CT3 GPL mt8167 DTS, clk-mt8167.c/clk-pll.c, pinctrl-mt8167.c and
i2c-mt65xx.c. Source-derived facts are independently implemented, not copied
GPL driver bodies. Physical BQ24160 variant is a board-source prerequisite;
family ID alone does not distinguish the watchdog-less A variant.

TI SLUSAO0H https://www.ti.com/lit/ds/symlink/bq24161.pdf : any I2C read/write
starts30 s WDT; register00 read may clear historical FAULT. This is active owner
instrumentation, never a passive/read-only diagnostic. Full raw first snapshot,
valid mask, timestamp and observed FAULT history are retained/logged per owner.
Kernel's first sample does not recover history already consumed by firmware.
Policy/fault/transaction/deadline failure stops servicing without repair/retry;
hardware DEFAULT fallback may change settings/safety state. This is not rollback.

Timer/EBS registration succeeds before any BQ read. Callback10 s, gap limit20 s,
transaction10 ms and full capture1 s limits use the shared Core. TPL_NOTIFY
serializes complete callback MMIO with EBS, which disarms/logs and seals the inline handoff record without charger MMIO.
No FW charger access after EBS. There is no assumed firmware pet in the kernel.
Measure LK->DXE and last-FW-pet->kernel-pet: either gap can still exceed30 s.
The existing TOPRGU recovery owner is independent; no attempt extends its deadline.

## P2 revision — source/host approved, runtime unqualified

Includes A04BR3ChargerHandoff.h. Device and firmware Core copies, plus the generic
DarwinBoot BoardHandoff.h protocol copy, must match. Verify with sync script
`--loader-root /absolute/DarwinBoot` as well as `--device-root`.

The kernel refuses a missing/stopped/invalid/late FW record BEFORE charger MMIO.
FW EBS seals state/FAULT history and physical last-pet/EBS counter with frequency
inside the loader-owned final ADT copy. A fresh kernel Owner cannot restart
feeding after a FW FAULT read consumed IC history. The remaining20 s owner-gap
bound survives scheduler/resource setup delays. Full normal/fault/late/corrupt
record host regressions execute the real callbacks and actual ADT generator.

The generic loader hook is defaultOFF and board-neutral; board DSC sets it to
A04BR3_CHARGER_OWNER. It attaches only the final tree, before map/EBS, and detaches
on loader failure before freeing the kernel window. No persistent variable or
unreserved physical memory is used. Pin reviewed loader/device/UEFI source together.
Chandra approved the loader source/host scope; target SDK/runtime proof remains pending.

Policy checks include reg06 VINDPM low6 bits and reg01 OTG_LOCK. EXT_CONF0x28 is
set/read back0x8001 for standard speed. Kernel touch/charger use the same short
register lock for shared mux RMW. Firmware holds TPL_NOTIFY; current other board
firmware does not write the same +0x80 word. No undocumented alias is assumed.

## protocol公開と失敗時cleanup

Chandraの最終source/host再レビューで承認済み。DXEはEBS event・timer event・
periodic timerを確立してからprotocolを公開する。公開を最後の失敗可能な操作とし、
4失敗経路すべてでprotocol参照不可・event全閉鎖・BQ read/petなしを実関数で検証した。
公開成功後のpolicy/resource拒否は、stopped ownerとしてEFI_SUCCESSを返し、callbackを
含むdriver imageの寿命を保つ。実機handoff・給電改善の承認を意味しない。

再現用loader forkの固定refは同dirの `CHARGER.sources.json` に記録する。
base側DarwinBoot gitlinkへの反映は主担当が行う。generic hookは既定OFFのまま。
