# A04BR3 bring-up recovery

The existing recovery code comes from origin/main commit
`45a04ea93e99e587c5c01f93e2b6b14e443679b3`. The local merge also retains
`d331be804970a5534e0fec973e0c6bdf993e3ad5` optional DarwinBoot diagnostics.
No second recovery driver or replacement PlatformSecLib is introduced.

`A04BR3_TEST_CYCLE` remains default 0 in the device DSC. The bring-up device
board explicitly passes 1. MT8167 PlatformInitialize disables TOPRGU at its
known physical MODE register (0x10007000), then immediately calls the existing
board hook. There is no memory-map lookup before that hook arms the 31-second
watchdog and attempts RTC_PDN1 bit13 with bounded WACS accesses/readback.
Code before PlatformInitialize, failed PMIC transactions, failed reset hardware,
or a later component deliberately disabling the watchdog are not proven safe.

The existing DXE driver uses TimerLib's increasing 64-bit counter for a
120-second elapsed feeding limit. It registers ExitBootServices before starting
its periodic timer. Failures start no feeding; the driver remains resident if
an event was registered. TPL_NOTIFY serializes timer MMIO with EBS. EBS stops
feeding without calling boot services, verifies RTC again, and arms a single
31-second interval. Existing XNU A04BR3AutoReset must take over before expiry;
that timing and the reset destination require UART/device evidence.

`A04BR3_EARLY_FAULT` is a source build define, default 0:

- 0: no intentional halt.
- 1: SEC halt, after successful RTC fastboot readback only.
- 2: DXE halt with the timer live, after successful RTC fastboot readback and
  successful event/timer setup only. The elapsed deadline must stop feeding.

Values 1/2 require A04BR3_TEST_CYCLE=1. RTC readback failure prints
`smoke refused` and continues the normal boot path; it never intentionally
halts into an unverified normal-boot reset loop. The hardware watchdog remains
armed before PWRAP and the existing kernel code can retry RTC setup later.
A host pass is not proof of RTC operation or entry into LK fastboot.

Run `python3 Platforms/Benesse/a04br3Pkg/Tests/check_recovery.py` for execution
of actual SEC/DXE C with host substitutes under ASan/UBSan, covering all three
source modes, RTC refusal, late/wrapped deadlines, EBS/timer ordering, and
registration failures. This does not emulate the hardware reset.

RAM600 layout changes are a separate coupled trial. Prefer proving recovery
on the established memory profile before qualifying a new memory profile.
Generate payloads through the official CLI from board configuration and reuse
prepared Data inputs unchanged. Do not patch ADT bytes in an existing payload.
