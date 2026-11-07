/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Camera-only private resources; no I2C/TOP/GPIO/PWRAP assignment. */
DefinitionBlock ("", "SSDT", 2, "BENESS", "A04CAM  ", 1) {
 Scope (_SB) { Device (CAM0) {
  Name (_HID, "MOU0020") Name (_UID, Zero) Name (_CCA, Zero)
  OperationRegion (HAND, SystemMemory, 0x12345678, 0x1000)
  Field (HAND,DWordAcc,NoLock,Preserve) { STAT,32,VERS,32,INVR,32,ERRS,32,PCWS,32,RATE,32,CHIP,32 }
  OperationRegion (SPMR,SystemMemory,0x1000660c,8)
  Field (SPMR,DWordAcc,NoLock,Preserve) { PST1,32,PST2,32 }
  OperationRegion (TOPR,SystemMemory,0x10000000,0x24)
  Field (TOPR,DWordAcc,NoLock,Preserve) { MUX0,32,Offset(0x20),GATE,32 }
  OperationRegion (APMR,SystemMemory,0x10018140,0x14)
  Field (APMR,DWordAcc,NoLock,Preserve) { PLLC,32,PLLP,32,Offset(0x10),PLPW,32 }
  OperationRegion (IMGR,SystemMemory,0x15000000,4)
  Field (IMGR,DWordAcc,NoLock,Preserve) { IMGC,32 }
  OperationRegion (SENR,SystemMemory,0x15008200,8)
  Field (SENR,DWordAcc,NoLock,Preserve) { PHAS,32,DIVS,32 }
  OperationRegion (GPDR,SystemMemory,0x10005010,4)
  Field (GPDR,DWordAcc,NoLock,Preserve) { DIRC,32 }
  OperationRegion (GPOR,SystemMemory,0x10005110,4)
  Field (GPOR,DWordAcc,NoLock,Preserve) { OUTC,32 }
  OperationRegion (GPMR,SystemMemory,0x10005390,4)
  Field (GPMR,DWordAcc,NoLock,Preserve) { MODC,32 }
  OperationRegion (GPCU,SystemMemory,0x10005440,4)
  Field (GPCU,DWordAcc,NoLock,Preserve) { MODK,32 }
  OperationRegion (MMCG,SystemMemory,0x14000100,4)
  Field (MMCG,DWordAcc,NoLock,Preserve) { SMIC,32 }
  Method (HSTA,0,Serialized) {
   If (LNotEqual(STAT,0x4134434d)) {Return(Zero)}
   If (LNotEqual(VERS,One)) {Return(Zero)} If (LNotEqual(INVR,0xbecbbcb2)) {Return(Zero)}
   If (LNot(And(PST1,0x20))) {Return(Zero)} If (LNot(And(PST2,0x20))) {Return(Zero)}
   If (And(GATE,2)) {Return(Zero)} If (LNot(And(GATE,0x10))) {Return(Zero)}
   If (LNotEqual(And(MUX0,0x38000),0x10000)) {Return(Zero)}
   If (LNot(And(PLLC,One))) {Return(Zero)} If (LNotEqual(And(PLPW,3),One)) {Return(Zero)}
   If (LNotEqual(PLLP,PCWS)) {Return(Zero)}
   If (LNotEqual(And(DIRC,0x30000),0x30000)) {Return(Zero)}
   If (LNotEqual(And(OUTC,0x30000),0x30000)) {Return(Zero)}
   If (And(MODC,0x7e00)) {Return(Zero)}
   If (LNotEqual(And(MODK,0x1c0),0x40)) {Return(Zero)}
   If (And(SMIC,One)) {Return(Zero)}
   If (And(IMGC,0x1e1)) {Return(Zero)}
   If (LNotEqual(And(PHAS,0xb0000047),0xa0000001)) {Return(Zero)}
   If (LNotEqual(And(DIVS,0x003f3f3f),0x00010001)) {Return(Zero)}
   Return(One)
  }
  Method (_STA,0,NotSerialized) {If(HSTA()){Return(0x0f)}Return(Zero)}
  Name (_CRS,ResourceTemplate() {
   Memory32Fixed(ReadWrite,0x15004000,0x00009000)
   Memory32Fixed(ReadWrite,0x10011000,0x00001000)
   Memory32Fixed(ReadWrite,0x15001000,0x00001000)
   Memory32Fixed(ReadOnly,0x87654321,0x00001000)
   Interrupt(ResourceConsumer,Level,ActiveHigh,Exclusive) {212}
  })
 } }
}
