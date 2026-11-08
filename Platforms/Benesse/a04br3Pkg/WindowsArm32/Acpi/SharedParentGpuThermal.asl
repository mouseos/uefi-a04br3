/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Opt-in combined parent. USB/AFE/MSDC2 resources belong to synthetic PDOs;
 * do not add ACPI child devices or duplicate their IRQ/memory ownership.
 * IO_RESOURCE_DESCRIPTOR carries level mode, not polarity. SharedIoIrqDxe
 * normalizes the board's level-low inputs to the GIC level-high contract. */
DefinitionBlock ("shared-parent-gpu-thermal.aml", "SSDT", 2, "BENESS", "A04GPUTH", 1)
{
    External (\_SB.GPU0, DeviceObj)
    Scope (_SB)
    {
        Device (SHR0)
        {
            Name (_HID, "BEN0001")
            Name (_UID, Zero)
            Name (_CCA, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10000000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10001000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10005000, 0x1000)
                Memory32Fixed (ReadWrite, 0x1000F000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10018000, 0x0714)
                // Optional single Windows SPM owner. Its private GPU service
                // restricts writes to MT8167 MFG/2D/ASYNC power controls.
                // It never writes PWRON_CONFIG_EN, CPU, wakeup or watchdog.
                Memory32Fixed (ReadWrite, 0x10006000, 0x1000)
                // Parent1101 delegates these two exclusive PIO ranges to its
                // single MT8167\Thermal PDO; no legacy ACPI thermal device.
                Memory32Fixed (ReadWrite, 0x1100D000, 0x1000)
                Memory32Fixed (ReadWrite, 0x11003000, 0x1000)
            })
        }
    }
    // Same provider-start dependency already used by the fixed SD-only
    // profile. Native factory must not race discovery of the V2 parent.
    Scope (\_SB.GPU0)
    {
        Name (_DEP, Package () {\_SB.SHR0})
    }
}
