/* Four CPU namespace for watchdog handoff testing. */
DefinitionBlock ("CpuParkingFourSsdt.aml", "SSDT", 2, "BENESS", "A04CPU4 ", 1)
{
    Scope (\_SB)
    {
        Device (CPU1) { Name (_HID, "ACPI0007") Name (_UID, 1) }
        Device (CPU2) { Name (_HID, "ACPI0007") Name (_UID, 2) }
        Device (CPU3) { Name (_HID, "ACPI0007") Name (_UID, 3) }
    }
}
