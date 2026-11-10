/* SPDX-License-Identifier: BSD-2-Clause-Patent */
DefinitionBlock ("PanelTiming.aml", "SSDT", 2, "BENESS", "A04DTIM ", 1)
{
 External (\_SB.DIS0, DeviceObj)
 Scope (\_SB.DIS0)
 {
  Name (TMB0, Buffer (64) {
   0x44,0x54,0x4d,0x31, 1,0,0,0, 64,0,0,0, 0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
  })
  Method (DTM0, 0, Serialized) { Return (TMB0) }
 }
}
