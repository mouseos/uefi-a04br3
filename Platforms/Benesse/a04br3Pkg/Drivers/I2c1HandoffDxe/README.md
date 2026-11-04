# I2C1 handoff experiment

`A04BR3_I2C1_HANDOFF=1` and `A04BR3_ACPI=1` include this board-scoped DXE;
it is off by default. The BeforeExitBootServices callback opens TOP1 bit 4,
selects the AHB-derived clock through INFRA bit 2, configures GPIO52/53 mode 1,
bias disabled and input enable, and initializes I2C1 for at most 100 kHz. It
checks its settings and logs status. The sequence is based on the board I2C2
DXE and stock MT8167 pinctrl/clock descriptions. No I2C slave transaction is
issued. The calculated parent and divider follow the I2C2 code; actual I2C1
clock, voltage and waveform are not measured.

The DXE allocates an ACPI NVS page, publishes a device SSDT with the page and
I2C1 as exclusive resources, and sets the page marker and timing only after
successful setup. Its HSTA method checks the marker and shared gate, mux and
pin configuration before Windows touches I2C1. A per-boot marker is not a
security boundary against firmware. Firmware must perform no I2C1 transfers
after this handoff; a future camera driver must use a shared controller owner
rather than separately claim I2C1. HSTA checks enable Windows D0 re-entry only
when the firmware-owned clock/pins remain configured. It cannot repair a lost
shared clock on resume. The handshake and device enumeration are unverified on
target hardware. Do not enable alongside a competing I2C1 ACPI provider.

Host mock: `cc -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I Platform/Benesse/A04BR3Pkg/tests/i2c1_mock Platform/Benesse/A04BR3Pkg/tests/i2c1_handoff_host.c -o /tmp/a04-i2c1-host && /tmp/a04-i2c1-host`.
This validates modeled register ordering and selected failure paths, not real
register behavior or concurrency. Neither a complete FD nor hardware has been
tested for this DXE.
