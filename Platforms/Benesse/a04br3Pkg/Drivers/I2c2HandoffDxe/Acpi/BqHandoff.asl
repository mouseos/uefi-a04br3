/* SPDX-License-Identifier: MIT
 * The DWord address is replaced by I2c2HandoffDxe before table installation.
 * BQHR returns a snapshot of the boot-local NVS record, not direct I2C data.
 */
DefinitionBlock ("", "SSDT", 2, "BENESS", "A04BQHR", 1)
{
    External (\_SB_.BAT0, DeviceObj)
    Scope (\_SB_.BAT0)
    {
        OperationRegion (BQRG, SystemMemory, 0xD1A2B3C4, 0x80)
        Field (BQRG, ByteAcc, NoLock, Preserve) { BQRD, 1024 }
        Method (BQHR, 0, Serialized) { Return (BQRD) }
    }
}
