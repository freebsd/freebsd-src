/*
 * RISC-V Bootstrap Entry Point
 * Mobile OS - Virtual Environment
 *
 * This is the very first code executed by the kernel
 * Executed in M-mode (Machine mode) by OpenSBI
 * Transitions to S-mode (Supervisor mode)
 *
 * Memory layout:
 * 0x80000000 - Kernel start (virtual)
 * 0x80000000 (physical) - Kernel entry point
 */

.section ".text.entry"
.global _start
.align 4

_start:
    /*
     * Entry point for RISC-V kernel
     * At this point:
     * - a0 (x10) = Hart ID
     * - a1 (x11) = Device tree pointer
     * - Stack is available from OpenSBI
     */

    /* Save boot parameters */
    mv s0, a0                   /* Save hart ID */
    mv s1, a1                   /* Save device tree pointer */

    /* Initialize stack pointer (a0 is hart id) */
    la sp, stack_top            /* Load stack address */
    li t0, 0x1000               /* Per-hart stack size (4KB) */
    mul t0, a0, t0              /* t0 = hart_id * 4KB */
    sub sp, sp, t0              /* Reserve stack space per hart */

    /* Clear BSS section */
    la t0, bss_start
    la t1, bss_end

bss_clear:
    bge t0, t1, bss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    jal zero, bss_clear

bss_done:
    /* Jump to kernel main */
    mv a0, s0                   /* Restore hart ID to a0 */
    mv a1, s1                   /* Restore device tree to a1 */
    jal ra, kernel_init         /* Jump to kernel initialization */

    /* Should never return */
halt:
    wfi                         /* Wait for interrupt */
    jal zero, halt              /* Loop forever if we do return */

/* Kernel initialization function (in riscv_hal.c) */
.extern kernel_init

/* Stack space: 64KB per hart (for 8 harts = 512KB) */
.section ".bss"
stack_space:
    .space 0x40000              /* 256KB total stack space */
stack_top:

/* BSS markers for cleanup */
.extern bss_start
.extern bss_end
