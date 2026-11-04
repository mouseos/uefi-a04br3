/* Experimental namespace for the CPU2/CPU3 parking protocol test. */
DefinitionBlock ("CpuParkingSsdt.aml", "SSDT", 2, "BENESS", "A04CPU  ", 1)
{
    Scope (\_SB)
    {
        Device (CPU2) { Name (_HID, "ACPI0007") Name (_UID, 2) }
        Device (CPU3) { Name (_HID, "ACPI0007") Name (_UID, 3) }
    }
}
