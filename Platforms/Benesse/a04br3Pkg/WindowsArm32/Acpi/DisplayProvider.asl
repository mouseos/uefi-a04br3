/* MT8167 display scanout resources used by the A04BR3 panel. The GPU core is
 * a separate ACPI device. Shared MMSYS, clock, SMI and power registers are
 * deliberately not claimed by this device. The Windows driver currently
 * uses firmware POST framebuffer ownership and does not program these blocks.
 * SPDX-License-Identifier: BSD-2-Clause-Patent */
DefinitionBlock ("DisplayProvider.aml", "SSDT", 2, "BENESS", "A04DISP ", 1)
{
    Scope (_SB)
    {
        Device (DIS0)
        {
            Name (_HID, "MOU0007")
            Name (_UID, Zero)
            // OVL/RDMA read system memory. ARM DMA devices must report
            // coherency; the board does not establish hardware snooping.
            // Zero conservatively requests non-coherent DMA treatment.
            Name (_CCA, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x14007000, 0x00001000) // OVL0
                Memory32Fixed (ReadWrite, 0x14009000, 0x00001000) // RDMA0
                Memory32Fixed (ReadWrite, 0x14012000, 0x00001000) // DSI0
                Memory32Fixed (ReadWrite, 0x14015000, 0x00001000) // display mutex
            })
        }
    }
}
