/*
 * RISC-V Bootstrap Entry Point
 * Mobile OS - QEMU virt / WSL nographic boot
 *
 * Single-hart boot (hart 0 only). All other harts are parked in a WFI
 * loop in machine mode by OpenSBI so we don’t have to manage them here.
 *
 * Stack: 16 KB per hart, allocated in .bss
 */

    .section ".text.entry"
    .global _start
    .align 4

_start:
    /* a0 = hart ID, a1 = DTB pointer */
    mv      s0, a0                  /* save hartid */
    mv      s1, a1                  /* save dtb */

    /* Set up stack (allocated in .bss as stack_space[]). */
    la      sp, _stack_top
    li      t0, 0x4000              /* 16 KB per hart */
    mul     t0, a0, t0
    sub     sp, sp, t0

    /* Only hart 0 runs the kernel; other harts park here forever. */
    beqz    s0, hart0_main
halt_other:
    wfi
    j       halt_other

hart0_main:
    /* Clear BSS */
    la      t0, __bss_start
    la      t1, __bss_end
    li      t2, 0
bss_loop:
    beq     t0, t1, bss_done
    sd      t2, 0(t0)
    addi    t0, t0, 8
    j       bss_loop
bss_done:

    /* Set trap vector */
    la      t0, trap_vector
    csrw    stvec, t0

    /* Enable interrupts (sstatus.SIE = 1) */
    li      t0, 0x02
    csrs    sstatus, t0

    /* Call kernel_init(hartid, dtb) */
    mv      a0, s0
    mv      a1, s1
    call    kernel_init

    /* Should never return; park in WFI */
hang:
    wfi
    j       hang

    .align  4
trap_vector:
    /* Minimal trap handler: just log to UART and halt. */
    addi    sp, sp, -16
    sd      ra, 0(sp)
    sd      a0, 8(sp)

    /* Print "TRAP" to UART (simple, no format). */
    li      a0, 0x54                 /* 'T' */
    call    uart_putc
    li      a0, 0x52                 /* 'R' */
    call    uart_putc
    li      a0, 0x41                 /* 'A' */
    call    uart_putc
    li      a0, 0x50                 /* 'P' */
    call    uart_putc
    li      a0, 0x0A                 /* '\n' */
    call    uart_putc

hang_forever:
    wfi
    j       hang_forever

    .section ".bss"
    .global _stack_space
_stack_space:
    .space  0x10000                  /* 64 KB total (16 KB x 4 harts) */

    .global _stack_top
_stack_top:
