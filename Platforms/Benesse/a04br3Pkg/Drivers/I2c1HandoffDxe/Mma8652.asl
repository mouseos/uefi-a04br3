/* SPDX-License-Identifier: MIT
 * Addresses 0x12345678 and 0x87654321 are build-time AML patch sentinels.
 */
DefinitionBlock ("", "SSDT", 2, "BENESS", "A04ACCL ", 1)
{
 Scope (_SB) {
  Device (ACC0) {
   Name (_HID, "MOU0010")
   Name (_UID, Zero)
   OperationRegion (HAND, SystemMemory, 0x12345678, 0x1000)
   Field (HAND, DWordAcc, NoLock, Preserve) { HSTS, 32 }
   OperationRegion (TOPR, SystemMemory, 0x10000024, 4)
   Field (TOPR, DWordAcc, NoLock, Preserve) { CGST, 32 }
   OperationRegion (INFR, SystemMemory, 0x10001080, 4)
   Field (INFR, DWordAcc, NoLock, Preserve) { ISEL, 32 }
   OperationRegion (GMOD, SystemMemory, 0x100053A0, 4)
   Field (GMOD, DWordAcc, NoLock, Preserve) { MODE, 32 }
   OperationRegion (GPUL, SystemMemory, 0x10005530, 4)
   Field (GPUL, DWordAcc, NoLock, Preserve) { PULL, 32 }
   OperationRegion (GIES, SystemMemory, 0x10005910, 4)
   Field (GIES, DWordAcc, NoLock, Preserve) { IENS, 32 }
   Method (HSTA, 0, Serialized) {
    If (LNotEqual (HSTS, 0x41303431)) { Return (Zero) }
    If (And (CGST, 0x10)) { Return (Zero) }
    If (And (ISEL, 0x04)) { Return (Zero) }
    If (LNotEqual (And (MODE, 0xFC0), 0x240)) { Return (Zero) }
    If (And (PULL, 0x30)) { Return (Zero) }
    If (LNot (And (IENS, One))) { Return (Zero) }
    Return (One)
   }
   Method (_STA, 0, NotSerialized) { If (HSTA ()) { Return (0x0F) } Return (Zero) }
   Name (_CRS, ResourceTemplate () {
    Memory32Fixed (ReadWrite, 0x1100A000, 0x1000)
    Memory32Fixed (ReadWrite, 0x87654321, 0x1000)
   })
  }
 }
}
