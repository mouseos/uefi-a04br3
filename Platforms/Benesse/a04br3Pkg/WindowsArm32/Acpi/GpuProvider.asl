/* MT8167 PowerVR GE8300 core resources from the board device tree.
 * Shared TOP/INFRA power registers are intentionally not claimed here.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
DefinitionBlock ("GpuProvider.aml", "SSDT", 2, "BENESS", "A04GPU  ", 1)
{
    Scope (_SB)
    {
        Device (GPU0)
        {
            Name (_HID, "MOU0006")
            Name (_UID, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x13000000, 0x00080000)
                Memory32Fixed (ReadWrite, 0x13FFE000, 0x00001000)
                // Device-tree GIC SPI 185 becomes GSIV 217.
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) { 217 }
            })
        }
    }
}
