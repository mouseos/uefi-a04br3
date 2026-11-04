/* Experimental one-CPU namespace and MT8167 eMMC resources. */
DefinitionBlock ("Dsdt.aml", "DSDT", 2, "BENESS", "A04BR3  ", 1)
{
    Scope (_SB)
    {
        Device (CPU0)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 0)
        }

        // Experimental ID for the independently developed Windows driver.
        // The interrupt is DT SPI 78 plus the GIC SPI base of 32.
        Device (EMMC)
        {
            Name (_HID, "MOU0001")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x11120000, 0x00001000)
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive)
                {
                    110
                }
            })
        }
    }
}
