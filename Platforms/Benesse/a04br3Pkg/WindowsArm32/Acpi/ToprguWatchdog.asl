/* MT8167 TOPRGU and UART0: optional watchdog handoff test resources. */
DefinitionBlock ("ToprguWatchdog.aml", "SSDT", 2, "BENESS", "A04RGU  ", 1)
{
    Scope (\_SB)
    {
        Device (RGU0)
        {
            Name (_HID, "MOU0005")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10007000, 0x00001000)
                Memory32Fixed (ReadWrite, 0x11005000, 0x00001000)
            })
        }
    }
}
