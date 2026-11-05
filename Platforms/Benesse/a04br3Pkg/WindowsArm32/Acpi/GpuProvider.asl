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
            // The board Linux device tree does not describe GPU DMA as
            // coherent. Conservatively request non-coherent DMA treatment;
            // this does not assert that any fabric path snoops CPU caches.
            Name (_CCA, Zero)
            // Read-only snapshots of shared power and clock registers. ACPI
            // owns these SystemMemory operation regions; the GPU driver does
            // not claim or write the shared register banks. A snapshot is not
            // a lease: the caller must separately hold a platform power owner.
            OperationRegion (GTOP, SystemMemory, 0x10000000, 0x44)
            Field (GTOP, DWordAcc, NoLock, Preserve)
            {
                GMX0, 32,
                GMX1, 32,
                Offset (0x40), GSLW, 32
            }
            OperationRegion (GSPM, SystemMemory, 0x10006000, 0x614)
            Field (GSPM, DWordAcc, NoLock, Preserve)
            {
                Offset (0x60c), GST1, 32, GST2, 32
            }
            Method (GPST, 0, Serialized)
            {
                Local0 = Buffer (20) {}
                CreateDWordField (Local0, 0, PS01)
                CreateDWordField (Local0, 4, PS02)
                CreateDWordField (Local0, 8, MX01)
                CreateDWordField (Local0, 12, MX02)
                CreateDWordField (Local0, 16, SLOW)
                PS01 = GST1
                PS02 = GST2
                MX01 = GMX0
                MX02 = GMX1
                SLOW = GSLW
                Return (Local0)
            }
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
