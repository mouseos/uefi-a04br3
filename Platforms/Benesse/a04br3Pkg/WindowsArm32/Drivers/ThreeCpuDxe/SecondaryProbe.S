/** AArch32 PSCI secondary-core smoke test. SPDX-License-Identifier: BSD-2-Clause-Patent **/
.arch_extension sec
.arm
.text
.align 2
.global A04SecondaryProbe
.type A04SecondaryProbe, %function
A04SecondaryProbe:
    movw r1, #0xbabe
    movt r1, #0xa04b
    str r1, [r0]
    dsb sy
    movw r0, #0x0002
    movt r0, #0x8400
    smc #0
1:  b 1b
.size A04SecondaryProbe, . - A04SecondaryProbe

.arch_extension virt
.arm
.global A04RequestA64Probe
.type A04RequestA64Probe, %function
A04RequestA64Probe:
    mov r1, #1
    hvc #0
    bx lr
.size A04RequestA64Probe, . - A04RequestA64Probe

.global A04RequestA32SecondaryProbe
.type A04RequestA32SecondaryProbe, %function
A04RequestA32SecondaryProbe:
    mov r2, #0
    hvc #0
    bx lr
.size A04RequestA32SecondaryProbe, . - A04RequestA32SecondaryProbe

.global A04RequestParkingProbe
.type A04RequestParkingProbe, %function
A04RequestParkingProbe:
    mov r2, #1
    hvc #0
    bx lr
.size A04RequestParkingProbe, . - A04RequestParkingProbe

.syntax unified
.thumb
.align 2
.global A04VectorMarkerStart
.global A04VectorMarkerEnd
A04VectorMarkerStart:
    movw r0, #0x5000
    movt r0, #0x1100
    movs r1, #0x21
1:  ldr r2, [r0, #0x14]
    lsls r2, r2, #26
    bpl 1b
    str r1, [r0]
2:  b 2b
A04VectorMarkerEnd:
