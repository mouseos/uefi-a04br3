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
#ifdef A04BR3_DISPLAY_GCE_RESOURCES
                // MT8167 DTS: non-secure GCE SPI 125 and OVL0 SPI 160. The GSI
                // values include the GIC SPI offset of 32. This opt-in only
                // describes resources; it does not claim a GCE thread or
                // enable interrupts, clocks, or display register writes.
                Memory32Fixed (ReadWrite, 0x1020A000, 0x00001000) // GCE
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) { 157 }
                // SPI 126 is the secure GCE interrupt, not claimed here.
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) { 192 }
#endif
            })
        }
    }
}
