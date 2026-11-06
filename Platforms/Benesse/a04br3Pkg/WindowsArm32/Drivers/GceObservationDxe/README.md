# GCE boot observation

`A04BR3_GCE_OBSERVATION=1` enables this board-only diagnostic; it is off by
default. The driver removes any stale volatile UEFI variable on entry. At
BeforeExitBootServices it reads TOPCKGEN TOP2 status (0x1000003c) and publishes
version 1 of `A04BR3GceObservation`, GUID
`b0f4cc26-0ad6-48e9-9474-e0372acf3241`. It also emits a DEBUG line. The
32-byte record has little-endian uint32 fields: version, size, valid mask,
TOP2, thread-enable, thread-status, thread-IRQ, ownership. Only valid bit 0
(TOP2) is set. Thread and IRQ values are UINT32_MAX; ownership is zero.

The existing display code already accesses TOPCKGEN. This diagnostic never
accesses the GCE registers, turns on the clock, clears an interrupt or event,
or changes either display layer. A clear gate bit is insufficient to prove
that nonsecure GCE MMIO is powered, accessible or exclusively owned. An EFI
variable is neither an authenticated claim nor evidence of secure firmware
quiescence. The Windows reader reports observations and never grants a lease.
