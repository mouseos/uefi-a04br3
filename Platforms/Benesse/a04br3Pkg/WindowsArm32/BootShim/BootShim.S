/* The servicing worker reserves CPU1; no watchdog-handoff or four-CPU path. */
#ifndef A04BR3_OFFLINE_WDT_UNBOUNDED
#define A04BR3_OFFLINE_WDT_UNBOUNDED 0
#endif
#ifndef A04BR3_EARLY_FDT_GUARD
#define A04BR3_EARLY_FDT_GUARD 0
#endif
#if A04BR3_EARLY_FDT_GUARD == 1 && A04BR3_A64_CPU_TEST != 1
#error Early FDT guard requires the board shim stack
#endif
#ifndef A04BR3_WORKER_NO_UART_RESET
#define A04BR3_WORKER_NO_UART_RESET 0
#endif
#ifndef A04BR3_WORKER_HEARTBEAT
#define A04BR3_WORKER_HEARTBEAT A04BR3_WORKER_NO_UART_RESET
#endif
#if A04BR3_WORKER_HEARTBEAT != 0 && A04BR3_WORKER_HEARTBEAT != 1
#error Invalid worker heartbeat option
#endif
#if A04BR3_WORKER_HEARTBEAT == 1 && A04BR3_A64_WDT_TEST != 1
#error Worker heartbeat requires watchdog worker
#endif
#if A04BR3_WORKER_NO_UART_RESET != 0 && A04BR3_WORKER_NO_UART_RESET != 1
#error Invalid worker UART reset option
#endif
#if A04BR3_WORKER_NO_UART_RESET == 1 && A04BR3_OFFLINE_WDT_UNBOUNDED != 1
#error UART reset exclusion is restricted to the offline worker experiment
#endif
#if A04BR3_WORKER_NO_UART_RESET == 1 && (A04BR3_KERNEL_MARKER != 0 || A04BR3_FB_MONITOR != 0)
#error Offline worker must not block on UART diagnostic output
#endif
#if A04BR3_OFFLINE_WDT_UNBOUNDED == 1 && (A04BR3_A64_WDT_TEST != 1 || A04BR3_A64_CPU_TEST != 1 || A04BR3_SMP_TEST != 1 || A04BR3_FOUR_CPU_TEST != 0 || A04BR3_WDT_HANDOFF_TEST != 0)
#error Offline worker requires 3 CPU SMP and exclusive watchdog ownership
#endif
#if A04BR3_OFFLINE_WDT_UNBOUNDED != 0 && A04BR3_OFFLINE_WDT_UNBOUNDED != 1
#error Invalid offline watchdog option
#endif
_Head:
	/* Set _Entry address */
	adr	x1, _Payload

	/* Jump to the real code */
	b	_Start

	/* Fake Linux kernel header */

_StackBase0: //has to be 0 on some devices
	/* Text Offset */
	.quad 0

_StackSize0:
	/* Image Size */
	.quad 0

	/* Flags */
	.quad 0

	/* Reserved */
	.quad 0

	/* Reserved */
	.quad 0

	/* Reserved */
	.quad 0

	/* ARM64 Magic */
	.ascii "ARM\x64"

	/* Reserved */
	.long 0

	.ascii "EDK2-MTK"

_StackBase:
	/* Text Offset */
	.quad UEFI_BASE

_StackSize:
	/* Image Size */
	.quad UEFI_SIZE

_Start:
    // Preserve LK arguments while cleaning the previous stage's cache state.
    mov x20, x0
    mov x21, x1
#if A04BR3_EARLY_FDT_GUARD == 1
    mov w0, #'0'
    bl _A04EarlyMark
#endif
    bl _CleanInvalidateDataCacheAll
#if A04BR3_EARLY_FDT_GUARD == 1
    mov w0, #'1'
    bl _A04EarlyMark
#endif
    mov x0, x20
    mov x1, x21
#if A04BR3_EARLY_FDT_GUARD == 1
    // LK's tags window is bounded before the first FDT load.  The temporary
    // stack is part of the already loaded shim, not the future FD or HOB.
    mrs x6, CurrentEL
    cmp x6, #8
    b.ne _Dead
    msr spsel, #1
    adr x6, _A64StackTop
    mov sp, x6
    movz x6, #0x4e00, lsl #16
    cmp x20, x6
    b.lo _A04FdtPointerFailure
    movz x7, #0x4e20, lsl #16
    sub x7, x7, #40
    cmp x20, x7
    b.hi _A04FdtPointerFailure
    tst x20, #7
    b.ne _A04FdtPointerFailure
    mov w0, #'2'
    bl _A04EarlyMark
    mov x0, x20
    movz x1, #0x4e20, lsl #16
    sub x1, x1, x20
    bl A04FdtGuard
    mov w19, w0
    mov w0, #'3'
    bl _A04EarlyMark
    mov w0, w19
    cbnz w0, _A04FdtFailure
    mov x0, x20
    mov x1, x21
#endif
	mov	x4, x1
	ldr	x5, _StackBase
	cmp	x4, x5
	beq	_Entry
	ldr	x6,	_StackSize

_CopyLoop:
	ldp	x2, x3, [x4], #16
	stp	x2, x3, [x5], #16
	subs	x6, x6, #16
	b.ne	_CopyLoop
	ldr	x5, _StackBase

_Entry:
#if A04BR3_EARLY_FDT_GUARD == 1
    mov w0, #'4'
    bl _A04EarlyMark
    mov x0, x20
    mov x1, x21
#endif
    // LK enters in AArch64 EL2. Enter ARM32 SVC with interrupts masked.
    mrs x6, CurrentEL
    cmp x6, #8
    b.ne _Dead
#if A04BR3_A64_CPU_TEST == 1
    adr x6, _A64Vectors
    msr vbar_el2, x6
    msr spsel, #1
    adr x6, _A64StackTop
    mov sp, x6
    isb
#endif
    msr hcr_el2, xzr
    mov x6, #3
    msr cnthctl_el2, x6
    msr cntvoff_el2, xzr
    mov x6, #0x33ff
    msr cptr_el2, x6
    msr hstr_el2, xzr
    msr elr_el2, x5
    mov x6, #0x1d3
    msr spsr_el2, x6
    isb
    eret

_Dead:
	/* We should never get here */
	b	_Dead
#if A04BR3_EARLY_FDT_GUARD == 1
// No stack, bounded polling and no writes while DLAB is selected.
// Clobbers x0-x4 only; stages are emitted as "~N\n".
_A04EarlyMark:
    movz x2, #0x5000
    movk x2, #0x1100, lsl #16
    ldr w1, [x2, #0x0c]
    tbnz w1, #7, 4f
    and w4, w0, #0xff
    lsl w4, w4, #8
    orr w4, w4, #0x7e
    mov w0, #10
    orr w4, w4, w0, lsl #16
    mov w3, #3
1:  mov w0, #4096
2:  ldr w1, [x2, #0x14]
    tbnz w1, #5, 3f
    subs w0, w0, #1
    b.ne 2b
    ret
3:  and w1, w4, #0xff
    str w1, [x2]
    lsr w4, w4, #8
    subs w3, w3, #1
    b.ne 1b
4:  ret
_A04FdtPointerFailure:
    mov w0, #0x10
_A04FdtFailure:
    // Never reset before the RTC fastboot flag reads back. If PWRAP/RTC or
    // watchdog enabling fails, remain halted for the operator to recover.
    bl A04FdtReport
    bl A04PrepareFastbootRecovery
    bl A04RecoveryReport
1:  wfe
    b 1b
#endif

// Based on the BSD-2-Clause-Patent SEC cache maintenance in UniXNU/uefi-sheng.
_CleanInvalidateDataCacheAll:
    dmb sy
    mrs x0, clidr_el1
    and x3, x0, #0x7000000
    lsr x3, x3, #23
    cbz x3, 5f
    mrs x1, id_aa64mmfr2_el1
    ubfx x8, x1, #20, #4
    mov x10, #0
1:  add x2, x10, x10, lsr #1
    lsr x1, x0, x2
    and x1, x1, #7
    cmp x1, #2
    b.lt 4f
    msr csselr_el1, x10
    isb
    mrs x1, ccsidr_el1
    and x2, x1, #7
    add x2, x2, #4
    cbz x8, 6f
    ubfx x4, x1, #3, #21
    ubfx x7, x1, #32, #24
    b 7f
6:  ubfx x4, x1, #3, #10
    ubfx x7, x1, #13, #15
7:  clz w5, w4
2:  mov x9, x4
3:  lsl x6, x9, x5
    orr x11, x10, x6
    lsl x6, x7, x2
    orr x11, x11, x6
    dc cisw, x11
    subs x9, x9, #1
    b.ge 3b
    subs x7, x7, #1
    b.ge 2b
4:  add x10, x10, #2
    cmp x3, x10
    b.gt 1b
5:  msr csselr_el1, xzr
    dsb sy
    isb
    ret

#if A04BR3_A64_CPU_TEST == 1
// Private EL2 vector for a bounded HVC32 secondary-core diagnostic.
.balign 2048
_A64Vectors:
.rept 12
    b _A64Unexpected
    .space 124
.endr
    b _A64Hvc
    .space 124
.rept 3
    b _A64Unexpected
    .space 124
.endr
_A64Unexpected:
    b _A64Unexpected
_A64Hvc:
    mrs x16, esr_el2
    ubfx x16, x16, #26, #6
    cmp x16, #0x12
    b.ne _A64Unexpected
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    mov x3, x0
#if A04BR3_A32_SECONDARY_TEST == 1
    cmp x1, #2
    b.eq _A64SecondaryRequest
    cmp x1, #3
    b.eq _A64SecondaryRequest
#endif
    movz x0, #0x0003
    movk x0, #0xc400, lsl #16
    mov x1, #1
    adr x2, _A64SecondaryProbe
    dc cvau, x2
    dsb sy
    ic ivau, x2
    dsb sy
    isb
    smc #0
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    eret
#if A04BR3_A32_SECONDARY_TEST == 1
_A64SecondaryRequest:
    mov x17, x2
    movz x0, #0x0003
    movk x0, #0xc400, lsl #16
    adr x2, _A64SecondaryA32Entry
#if A04BR3_PARK_TEST == 1
    cmp x17, #1
    b.ne 41f
    adr x2, _A64SecondaryParkEntry
41:
#endif
    dc cvau, x2
    dsb sy
    ic ivau, x2
    dsb sy
    isb
    smc #0
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    eret
_A64SecondaryA32Entry:
    // Context points to reserved memory: marker at 0, ARM32 entry at 4.
    movz w1, #0x0064
    movk w1, #0xa042, lsl #16
    str w1, [x0]
    dsb sy
    mrs x1, CurrentEL
    cmp x1, #8
    b.ne _A64SecondaryA32Halt
    ldr w2, [x0, #4]
    msr hcr_el2, xzr
    mov x1, #3
    msr cnthctl_el2, x1
    msr cntvoff_el2, xzr
    mov x1, #0x33ff
    msr cptr_el2, x1
    msr hstr_el2, xzr
    msr elr_el2, x2
    mov x1, #0x1d3
    msr spsr_el2, x1
    isb
    eret
_A64SecondaryA32Halt:
    wfe
    b _A64SecondaryA32Halt
#if A04BR3_PARK_TEST == 1
_A64SecondaryParkEntry:
    mov x19, x0
    mrs x1, CurrentEL
    cmp x1, #8
    b.ne _A64SecondaryA32Halt
    bl _CleanInvalidateDataCacheAll
    ic iallu
    dsb sy
    isb
_A64EnterA32Parking:
    mov x0, x19
    add x2, x19, #0x800
    msr hcr_el2, xzr
    mov x1, #3
    msr cnthctl_el2, x1
    msr cntvoff_el2, xzr
    mov x1, #0x33ff
    msr cptr_el2, x1
    msr hstr_el2, xzr
    msr elr_el2, x2
    mov x1, #0x1d3
    msr spsr_el2, x1
    isb
    eret
#endif
#endif
_A64SecondaryProbe:
    mov x19, x0
#if A04BR3_FB_MONITOR == 1
    mov x29, #0
#endif
    movz w1, #0xbeef
    movk w1, #0xa064, lsl #16
    str w1, [x0]
    dsb sy
#if A04BR3_A64_WDT_TEST == 1
    // CPU1 is absent from this experimental MADT. Bound feeding using the
    // physical counter even if CPU0 stops. Zero counter frequency fails safe.
    mrs x4, cntfrq_el0
    cbz x4, 2f
    mrs x5, cntpct_el0
    mov x6, #A04BR3_A64_WDT_SECONDS
    mul x6, x4, x6
    movz x7, #0x7008
    movk x7, #0x1000, lsl #16
    movz w8, #0x1971
    adr x13, _A64ResetCommand
    mov x14, #0
    movz x15, #0x5000
    movk x15, #0x1100, lsl #16
1:  mrs x9, cntpct_el0
#if A04BR3_OFFLINE_WDT_UNBOUNDED != 1
    sub x10, x9, x5
    cmp x10, x6
    b.hs 2f
#endif
#if A04BR3_WDT_HANDOFF_TEST == 1
    ldr w16, [x19, #40]
    cbnz w16, 52f
#endif
    str w8, [x7]
    dsb sy
#if A04BR3_WORKER_HEARTBEAT == 1
    // Reserved probe page, offset 44: completed feed writes, not proof that
    // the watchdog is enabled or that any other reset source is excluded.
    ldr w16, [x19, #44]
    add w16, w16, #1
    str w16, [x19, #44]
    dsb sy
#endif
#if A04BR3_WDT_HANDOFF_TEST == 1
52:
#endif
    mov x11, #5
    madd x12, x4, x11, x9
3:
#if A04BR3_WORKER_NO_UART_RESET != 1
    // Honor the existing reset command even after Boot Services exit.
    // Never read RBR when DLAB is set during UART reconfiguration.
    ldr w16, [x15, #0x0c]
    tbnz w16, #7, 4f
    ldr w16, [x15, #0x14]
    tbz w16, #0, 4f
    ldr w17, [x15]
    and w17, w17, #0xff
#if A04BR3_FB_MONITOR == 1
    // The reset and framebuffer commands share the first seven bytes.
    cmp x14, #7
    b.ne 19f
#if A04BR3_WDT_HANDOFF_TEST == 1
    cmp w17, #'W'
    b.ne 53f
    adr x13, _A64WdtPauseCommand
    b 19f
53:
#endif
    cmp w17, #'F'
    b.ne 19f
    adr x13, _A64FbCommand
19: cmp x14, #8
    b.ne 27f
    cmp w17, #'3'
    b.ne 27f
    adr x13, _A64Fb3Command
27:
#endif
#if A04BR3_WDT_HANDOFF_TEST == 1
    cmp x14, #9
    b.ne 54f
    cmp w17, #'R'
    b.ne 54f
    adr x16, _A64WdtPauseCommand
    cmp x13, x16
    b.ne 54f
    adr x13, _A64WdtResumeCommand
54:
#endif
    ldrb w18, [x13, x14]
    cmp w17, w18
    b.ne 5f
    add x14, x14, #1
    cmp x14, #10
    b.ne 4f
#if A04BR3_WDT_HANDOFF_TEST == 1
    adr x16, _A64WdtPauseCommand
    cmp x13, x16
    b.ne 55f
    mov w16, #1
    str w16, [x19, #40]
    dsb sy
    mov w21, #1
    mov x24, #8
    bl _A64ReportKernelChange
    adr x13, _A64ResetCommand
    mov x14, #0
    b 4f
55: adr x16, _A64WdtResumeCommand
    cmp x13, x16
    b.ne 56f
    str wzr, [x19, #40]
    dsb sy
    mov w21, #0
    mov x24, #8
    bl _A64ReportKernelChange
    adr x13, _A64ResetCommand
    mov x14, #0
    b 4f
56:
#endif
#if A04BR3_FB_MONITOR == 1
    adr x16, _A64FbCommand
    cmp x13, x16
    b.eq 28f
    adr x16, _A64Fb3Command
    cmp x13, x16
    b.ne 20f
    mov x17, #3
    b 29f
28: mov x17, #0
29: adr x13, _A64ResetCommand
    mov x14, #0
    bl _A64DumpFramebuffer
    b 4f
20:
#endif
    movz w16, #0x1209
    str w16, [x7, #12]
    dsb sy
    mov x14, #0
    b 4f
5:  mov x14, #0
    adr x13, _A64ResetCommand
    cmp w17, #'A'
    cinc x14, x14, eq
#endif
4:
#if A04BR3_FOUR_CPU_TEST == 1
    // CPU1 keeps the watchdog/UART worker until Windows requests its mailbox.
_A64HandoffTestStart:
    ldr w20, [x19, #36]
    cbz w20, 57f
    ldr w21, [x20]
    cmp w21, #1
    b.ne 57f
    ldr w21, [x20, #12]
    cbnz w21, 57f
    ldr w21, [x20, #8]
    cbz w21, 57f
    tst w21, #3
    b.ne 57f
_A64HandoffTestCurrentEL:
    mrs x22, CurrentEL
    cmp x22, #8
    b.ne 57f
    // The Windows boot-start owner can only take over after CPU1 leaves.
    // Do not relinquish the last known feeder if TOPRGU has meanwhile been
    // disabled by another actor; a kick to a disabled watchdog is no guard.
    ldur w16, [x7, #-8]      // TOPRGU_MODE; x7 points to TOPRGU_RESTART.
    tbnz w16, #0, 58f
    ldr w16, [x19, #48]      // One-shot disabled-state diagnostic.
    cbnz w16, 57f
    mov w16, #1
    str w16, [x19, #48]
    dsb sy
    mov w26, #'6'
    bl _A64HandoffMark
    b 57f
58: mov w26, #'5'
    bl _A64HandoffMark
    // Start a full hardware timeout before relinquishing the worker. If the
    // Windows watchdog does not take over, RTC routes the reset to fastboot.
    str w8, [x7]
    dsb sy
    mov x19, x20
_A64HandoffTestInvalidate:
    ic iallu
    dsb sy
    isb
    b _A64EnterA32Parking
57:
_A64HandoffTestSkip:
#endif
#if A04BR3_KERNEL_MARKER == 1
    // Slots are armed only after the BSP flushes their addresses and baselines.
    ldr w20, [x19, #20]
    cbz w20, 8f
    ldr w20, [x19, #4]
    cbz w20, 6f
    ldr w21, [x20]
    ldr w22, [x19, #8]
    cmp w21, w22
    b.eq 6f
    str wzr, [x19, #4]
    mov x24, #1
    bl _A64ReportKernelChange
6:  ldr w20, [x19, #12]
    cbz w20, 8f
    ldr w21, [x20]
    ldr w22, [x19, #16]
    cmp w21, w22
    b.eq 8f
    str wzr, [x19, #12]
    mov x24, #2
    bl _A64ReportKernelChange
8:
#if A04BR3_SMP_TEST == 1
    ldr w20, [x19, #28]
    cbz w20, 50f
    ldr w21, [x20]
    movz w22, #0xc0de
    movk w22, #0xa04b, lsl #16
    cmp w21, w22
    b.ne 50f
    str wzr, [x19, #28]
    mov x24, #6
    bl _A64ReportKernelChange
50: ldr w20, [x19, #32]
    cbz w20, 51f
    ldr w21, [x20]
    movz w22, #0xc0de
    movk w22, #0xa04b, lsl #16
    cmp w21, w22
    b.ne 51f
    str wzr, [x19, #32]
    mov x24, #7
    bl _A64ReportKernelChange
51:
#endif
#endif
#if A04BR3_FB_MONITOR == 1
    // Enabled by BSP only after successful ExitBootServices. Sample physical
    // scanout RAM without altering it; each report covers 256 spaced pixels.
    ldr w20, [x19, #24]
    cbz w20, 17f
    mrs x9, cntpct_el0
    cmp x9, x29
    b.lo 17f
    add x29, x9, x4
    movz x20, #0x0000
    movk x20, #0xbe9c, lsl #16
    mov x21, #0
    mov x22, #256
    mov x27, #15872
    mov w28, wzr
14: ldr w26, [x20]
    and w26, w26, #0xffffff
    cmp w26, #0
    cinc w21, w21, ne
    eor w28, w28, w26
    add x20, x20, x27
    subs x22, x22, #1
    b.ne 14b
    mov w20, w28
    mov x24, #3
    bl _A64ReportKernelChange
    mov w21, w20
    mov x24, #4
    bl _A64ReportKernelChange
    movz x20, #0x702c
    movk x20, #0x1400, lsl #16
    ldr w21, [x20]
    mov x24, #5
    bl _A64ReportKernelChange
17:
#endif
    mrs x9, cntpct_el0
    cmp x9, x12
    b.lo 3b
    b 1b
#if A04BR3_FOUR_CPU_TEST == 1
// Bounded UART trace before CPU1 leaves firmware. No stack; never write DLAB.
// w26 selects '~5' (handoff) or '~6' (watchdog disabled). x20 (mailbox),
// x4 (counter frequency), x7/x8 (watchdog), and x12 (deadline) are preserved.
_A64HandoffMark:
    ldr w16, [x15, #0x0c]
    tbnz w16, #7, 64f
    mov w22, #3
    mov w21, #'~'
60: mov w17, #4096
61: ldr w16, [x15, #0x14]
    tbnz w16, #5, 62f
    subs w17, w17, #1
    b.ne 61b
    ret
62: str w21, [x15]
    dsb sy
    subs w22, w22, #1
    b.eq 64f
    cmp w22, #2
    csel w21, w26, wzr, eq
    cmp w22, #1
    mov w16, #10
    csel w21, w16, w21, eq
    b 60b
64: ret
#endif
#if A04BR3_KERNEL_MARKER == 1
_A64ReportKernelChange:
    mov x23, x30
    adr x25, _A64KernelText
7:  ldrb w26, [x25], #1
    cbz w26, 9f
    bl _A64PutChar
    b 7b
9:  add w26, w24, #'0'
    bl _A64PutChar
    mov w26, #'='
    bl _A64PutChar
    mov x27, #28
10: lsr w26, w21, w27
    and w26, w26, #15
    cmp w26, #10
    b.lo 11f
    add w26, w26, #('A' - 10)
    b 12f
11: add w26, w26, #'0'
12: bl _A64PutChar
    subs x27, x27, #4
    b.ge 10b
    mov w26, #10
    bl _A64PutChar
    mov x30, x23
    ret
_A64PutChar:
13: ldr w28, [x15, #0x14]
    tbz w28, #5, 13b
    str w26, [x15]
    ret
#if A04BR3_FB_MONITOR == 1
// One-bit physical framebuffer dump, 800x1280 pixels, packed left-to-right.
// Threshold the sum of RGB channels to retain dark text on bright dialogs.
// The caller requests it explicitly; no persistent state or framebuffer writes.
_A64DumpFramebuffer:
    mov x18, x30
    adr x25, _A64FbBegin
    cbz x17, 21f
    adr x25, _A64Fb3Begin
21: ldrb w26, [x25], #1
    cbz w26, 22f
    bl _A64PutChar
    b 21b
22: cbnz x17, 30f
    movz x25, #0x0000
    movk x25, #0xbe9c, lsl #16
    b 31f
30: movz x25, #0x8000
    movk x25, #0xbeda, lsl #16
31: mov x23, #0
    mov x22, #0
    mov x24, #0
    movz x20, #0xa000
    movk x20, #0x000f, lsl #16 // 1,024,000 pixels
23: ldr w21, [x25], #4
    and w0, w21, #0xff
    ubfx w1, w21, #8, #8
    add w0, w0, w1
    ubfx w1, w21, #16, #8
    add w0, w0, w1
    cmp w0, #384
    cset w21, hs
    orr w22, w21, w22, lsl #1
    add x24, x24, #1
    cmp x24, #4
    b.ne 25f
    and w26, w22, #15
    cmp w26, #10
    b.lo 24f
    add w26, w26, #('A' - 10)
    b 26f
24: add w26, w26, #'0'
26: bl _A64PutChar
    mov x24, #0
    mov x22, #0
25: add x23, x23, #1
    cmp x23, x20
    b.lo 23b
    mov w26, #10
    bl _A64PutChar
    mov x30, x18
    ret
_A64FbBegin:
    .asciz "\nA04FBHEX 800 1280 "
_A64Fb3Begin:
    .asciz "\nA04FB3HEX 800 1280 "
_A64FbCommand:
    .ascii "A04BR3!FB!"
_A64Fb3Command:
    .ascii "A04BR3!F3!"
    .balign 4
#endif
_A64KernelText:
    .asciz "\nA04TRACE kernel-global slot "
    .balign 4
#endif
_A64ResetCommand:
    .ascii "A04BR3!RST"
    .balign 4
#if A04BR3_WDT_HANDOFF_TEST == 1
_A64WdtPauseCommand:
    .ascii "A04BR3!WDP"
_A64WdtResumeCommand:
    .ascii "A04BR3!WDR"
    .balign 4
#endif
2:
#endif
    movz x0, #0x0002
    movk x0, #0x8400, lsl #16
    smc #0
1:  wfe
    b 1b
.balign 16
_A64Stack:
    .space 4096
_A64StackTop:
#endif
.text
.align 4
#if A04BR3_EARLY_FDT_GUARD == 1
.section .payload,"ax"
.balign 16
#endif

_Payload:
	/* Your code will get ran right after this binary */
