/* Experimental polling BU21025 provider. No interrupt or wake is claimed.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
DefinitionBlock ("TouchProvider.aml", "SSDT", 2, "BENESS", "A04TCH  ", 1)
{
    Scope (_SB)
    {
        Device (TCH0)
        {
            Name (_HID, "MOU0004")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x10000000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10001000, 0x1000)
                Memory32Fixed (ReadWrite, 0x10005000, 0x1000)
                Memory32Fixed (ReadWrite, 0x11009000, 0x1000)
            })
        }
    }
}
