# Optional Windows GPU domain owner resource profile

`A04BR3_GPU_DOMAIN_OWNER=1` is default OFF and requires the existing complete
`A04BR3_SHARED_IO_PROFILE=1`. It selects AcpiTablesSharedGpuDomain.inf instead
of the normal shared IO table storage. The storage FILE_GUID remains exactly
7E374E25-8E01-4FEE-87F2-390C23C606CD, matching the existing loader PCD.

Only SHR0/BEN0001 gains the explicit exclusive 0x10006000/0x1000 SPM resource.
GPU0 core/MFG/IRQ resources and GPST are unchanged. No AML writes are added.
The matched parent1014 SPM profile is mandatory; parent1008 rejects the sixth
window rather than mapping an unassigned physical constant.

The Windows parent restricts writes to MT8167 MFG(0x214), MFG_2D(0x2c0) and
MFG_ASYNC(0x2c4) power controls. It only reads PWRON_CONFIG_EN(0x0), refuses
service if the existing handoff did not enable it, and never rewrites its
shared unlock/key bits. CPU, watchdog, wakeup, TOP/PLL and PMIC are not written
by this resource profile. This does not prove firmware relinquishes every
other SPM register: non-GPU fields remain outside the service's write scope.

This machine-only change does not edit common EDK2 modules, does not modify
the default ACPI profile, and has not been flashed. ASL standalone compilation
is separate from a complete FD build and hardware qualification.
