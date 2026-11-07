// Source integration proposal only. Do not load beside another MSDC1 owner.
// Keeps all shared registers read-only; platform must retain SD's existing
// VMC/VMCH 3.3V supplies and mux2 source clock while this device is in D0.
DefinitionBlock ("", "SSDT", 2, "MOUSEO", "MTKSD001", 1)
{
    External (\_SB.SHR0, DeviceObj)
    Scope (\_SB)
    {
        Device (SDH1)
        {
            Name (_HID, "MOU0013") // project-assigned ACPI binding, not an IC ID
            Name (_UID, One)
            Name (_DEP, Package () {\_SB.SHR0}) // actual FD must match this namespace
            Name (_DDN, "MediaTek MT8167 MSDC1 microSD Host")
            Name (_CCA, Zero) // PIO profile never creates a DMA mapping
            Method (_STA, 0) { Return (0x0F) } // socket exists even when empty
            Name (_CRS, ResourceTemplate () {
                Memory32Fixed (ReadWrite, 0x11130000, 0x1000)
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) {111}
            })
            // Linux pinctrl: DIN port=(pin>>4)<<4, bit=pin&15. GPIO20 is
            // DIN+0x10 bit4, not bit20 at DIN0. Native mmc_of_parse/slot-gpio
            // with flags0 and no cd-inverted yields present=raw (active high).
            OperationRegion (CDRG, SystemMemory, 0x10005210, 4)
            Field (CDRG, DWordAcc, NoLock, Preserve) {Offset(0), ,4, CD20,1}
            OperationRegion (DIRG, SystemMemory, 0x10005010, 4)
            Field (DIRG, DWordAcc, NoLock, Preserve) {Offset(0), ,4, DI20,1}
            OperationRegion (MOD0, SystemMemory, 0x10005340, 4)
            Field (MOD0, DWordAcc, NoLock, Preserve) {MD20,3}
            OperationRegion (MOD1, SystemMemory, 0x10005440, 4)
            Field (MOD1, DWordAcc, NoLock, Preserve) {Offset(1), ,4, M104,3}
            OperationRegion (MOD2, SystemMemory, 0x10005450, 4)
            Field (MOD2, DWordAcc, NoLock, Preserve) {M105,3,M106,3,M107,3,M108,3,M109,3}
            OperationRegion (TMUX, SystemMemory, 0x10000000, 4)
            Field (TMUX, DWordAcc, NoLock, Preserve) {Offset(2), ,4, SDMX,3}
            Method (SDCD, 0, Serialized) { Return (CD20) }
            Method (SDCF, 0, Serialized)
            {
                If (LOr (LNotEqual (MD20, Zero), LNotEqual (DI20, Zero))) {Return (Zero)}
                If (LOr (LNotEqual (M104, One), LNotEqual (M105, One))) {Return (Zero)}
                If (LOr (LNotEqual (M106, One), LNotEqual (M107, One))) {Return (Zero)}
                If (LOr (LNotEqual (M108, One), LNotEqual (M109, One))) {Return (Zero)}
                If (LNotEqual (SDMX, 2)) {Return (Zero)}
                Return (One)
            }
            // No _PSx/_PRx/rails mutation is fabricated. Integrator must bind
            // real platform power ownership; SDCF proves mux/pins only.
        }
    }
}
