/** Experimental ARM MP parking loop, copied to each reserved mailbox page.
    SPDX-License-Identifier: BSD-2-Clause-Patent **/
.syntax unified
.arm
.text
.align 2
.global A04ParkingStart
.global A04ParkingEnd
.type A04ParkingStart, %function
A04ParkingStart:
    /* r0 = physical, 4 KiB aligned mailbox; no stack or external references. */
    cpsid aif
    mrc p15, 0, r2, c1, c0, 0
    bic r2, r2, #1
    bic r2, r2, #4
    bic r2, r2, #0x1000
    dsb sy
    mcr p15, 0, r2, c1, c0, 0
    isb
    mrc p15, 0, r4, c0, c0, 5
    and r4, r4, #0xff
    movw r3, #0x0000
    movt r3, #0x1032
    mov r2, #0xff
    str r2, [r3, #4]       /* GICC_PMR */
    mov r2, #1
    str r2, [r3]           /* non-secure GICC_CTLR */
    dsb sy
    movw r2, #0xcafe
    movt r2, #0xa04b
    str r2, [r0, #0xffc]  /* private firmware-half parked marker */
    dsb sy
1:  ldr r2, [r0]
    cmp r2, r4
    bne 2f
    ldr r3, [r0, #12]
    cmp r3, #0             /* ARM32 entry must fit in 32 bits. */
    bne 2f
    ldr r3, [r0, #8]
    cmp r3, #0
    beq 2f
    tst r3, #3             /* The protocol requires ARM state. */
    bne 2f
    mov r2, #0
    str r2, [r0, #8]
    str r2, [r0, #12]
    dsb sy
    movw r2, #0xc0de
    movt r2, #0xa04b
    str r2, [r0, #0xffc]  /* private proof that a jump was acknowledged */
    dsb sy
    bx r3
2:  dsb sy
    wfi
    b 1b
A04ParkingEnd:
.size A04ParkingStart, . - A04ParkingStart
