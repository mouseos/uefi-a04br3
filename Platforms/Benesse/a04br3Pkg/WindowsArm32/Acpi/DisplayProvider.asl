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
#ifdef A04BR3_UNIFIED_DISPLAY356
            // CT3 mt8167.dtsi larb0=0x14016000; m4u_reg.h MMU_EN=0xfc0.
            // Read-only platform proof. Windows must not infer POST PA=DMA.
            OperationRegion (DMMU, SystemMemory, 0x14016FC0, 4)
            Field (DMMU, DWordAcc, NoLock, Preserve) { DME0, 32 }
            OperationRegion (DPOL, SystemMemory, 0x10200634, 4)
            Field (DPOL, DWordAcc, NoLock, Preserve) { DPI5, 32 }
            OperationRegion (DVID, SystemMemory, 0x14012014, 4)
            Field (DVID, DWordAcc, NoLock, Preserve) { DVM0, 32 }
            Method (DMS0, 0, Serialized)
            {
                Local0 = Buffer (32) {}
                CreateDWordField (Local0, 0, DMAG)
                CreateDWordField (Local0, 4, DVER)
                CreateDWordField (Local0, 8, MMEN)
                CreateDWordField (Local0, 12, POL5)
                CreateDWordField (Local0, 16, DWID)
                CreateDWordField (Local0, 20, DHEI)
                CreateDWordField (Local0, 24, DPIT)
                CreateDWordField (Local0, 28, DMOD)
                DMAG = 0x36354D44
                DVER = One
                MMEN = DME0
                POL5 = DPI5
                DWID = 800
                DHEI = 1280
                DPIT = 3200
                If (DVM0 == 2) { DMOD = One }
                Return (Local0)
            }
#endif
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x14007000, 0x00001000) // OVL0
                Memory32Fixed (ReadWrite, 0x14009000, 0x00001000) // RDMA0
                Memory32Fixed (ReadWrite, 0x14012000, 0x00001000) // DSI0
                Memory32Fixed (ReadWrite, 0x14015000, 0x00001000) // display mutex
                // GOP/MemoryMapLib reserve the LK scanout at this address.
                // Assign it to the display PDO, so Windows can match POST
                // ownership using real framebuffer resource containment.
                // Do not give this RAM to GPU0 or the shared platform parent.
                Memory32Fixed (ReadWrite, 0xBE9C0000, 0x01000000) // POST framebuffer
#ifdef A04BR3_DISPLAY_BRIGHTNESS_RESOURCES
                // Linux mt8167.dtsi disp_pwm0; exclusive display brightness owner.
                // GPIO1 enable, TOPCK and MMSYS remain with their existing owners.
                Memory32Fixed (ReadWrite, 0x1100F000, 0x00001000) // DISP_PWM0
#endif
#ifdef A04BR3_UNIFIED_DISPLAY356
                // SYSIRQ inverts CT3 LEVEL_LOW SPI160/162; generic GIC high.
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 192 }
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 194 }
#elif defined(A04BR3_DISPLAY_GCE_RESOURCES)
                // OVL0 SPI 160, translated to GSI 192.
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) { 192 }
#endif
            })
        }
#ifdef A04BR3_DISPLAY_GCE_RESOURCES
        // GCE is a distinct bus master. Its own PDO and _CCA permit a DMA
        // adapter to be requested without using the OVL device's adapter.
        // Describing resources does not establish event/thread/clock ownership.
        Device (GCE0)
        {
            Name (_HID, "MOU0099")
            Name (_UID, Zero)
            Name (_CCA, Zero)
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0x1020A000, 0x00001000)
                // MT8167 DTS non-secure GCE SPI 125, GSI 157.
                // SPI 126 is secure and is not claimed.
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) { 157 }
            })
        }
#endif
    }
}
