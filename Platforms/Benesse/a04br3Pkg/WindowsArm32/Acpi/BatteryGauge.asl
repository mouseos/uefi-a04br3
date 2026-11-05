/* Optional LC709203F / MT8167 I2C2 owner for the Windows RT diagnostic.
 * Exclusively claims the controller; no concurrent charger/SPB owner allowed.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
DefinitionBlock ("BatteryGauge.aml", "SSDT", 2, "BENESS", "A04BATT ", 1)
{
    Scope (_SB)
    {
        Device (BAT0)
        {
            Name (_HID, "MOU0008")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x1100B000, 0x00001000)
            })
        }
    }
}
