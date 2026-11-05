/* Offline namespace fixture only. Do not install or merge with legacy nodes. */
DefinitionBlock ("shared-parent-apmixed.aml", "SSDT", 2, "BENESS", "A04SHR  ", 1)
{
    Scope (_SB)
    {
        Device (SHR0)
        {
            Name (_HID, "BEN0001")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10000000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10001000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10005000, 0x1000)
                Memory32Fixed (ReadWrite, 0x1000F000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10018000, 0x0710)
            })
        }
    }
}
