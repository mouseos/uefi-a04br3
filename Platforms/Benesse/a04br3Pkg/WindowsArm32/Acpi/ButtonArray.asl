/* Optional notification-only button array; hardware producer is separate.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 * No GPIO resources are claimed; this does not provide hardware wake. */
DefinitionBlock ("ButtonArray.aml", "SSDT", 2, "BENESS", "A04BTN  ", 1)
{
    Scope (_SB)
    {
        Device (BTT0)
        {
            Name (_HID, "MOU0002")
            Name (_CID, "PNP0C40")
            Name (_UID, Zero)
        }
    }
}
