# SD-only ACPI profile

`A04BR3_SD_PIO_PROFILE=0` is the default. Enable through
`A04_DIAGNOSTIC_SD_PIO_PROFILE=1` with the existing SharedIo profile.
The DSC and FDF select one ACPI storage producer, retaining the loader PCD GUID.
This profile adds only SdPio.asl; the existing nine tables remain unchanged.

SdPio.asl is byte-identical to the with-DEP source at drivers commit
9e07f0a6cdf20f6efe9a4a75ed5f42fcd6ee583c. See sd-pio-contract.json.
Match parent 1010 and SD 3 from that signed tuple; thermal is disabled in
that Windows tuple. Existing firmware thermal observation is not a release
of thermal ownership. Do not substitute parent 1012.

_DEP on \_SB.SHR0 establishes enumeration order only. The Windows parent owns
the shared lock, PWRAP and SD power lease; the consumer owns its private MSDC1
PIO state. Firmware adds no PMIC, SPM, gate or SYSIRQ writes. Only an already
clocked UEFI-ready SD handoff is supported by this tuple. Unknown DMA state
must not be resumed by ungating clocks. GPIO20 and configuration checks are
read-only; IRQ resource publication does not demonstrate interrupt operation.

Build from the fixed 341894cd ARM32 profile, preserving every previous define,
with only the SD flag added. Do not merge RTC candidates into this profile.
Host validation and boot packaging do not establish hardware operation.
Flash and hardware validation belong to the main owner.
