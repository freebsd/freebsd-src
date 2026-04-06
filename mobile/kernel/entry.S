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
    /* Set up trap vector */
    la t0, trap_vector
    csrw stvec, t0

    /* Enable interrupts */
    li t0, 0x222          /* SPIE, SIE */
    csrs sstatus, t0

    /* Jump to kernel main */
    mv a0, s0                   /* Restore hart ID to a0 */
    mv a1, s1                   /* Restore device tree to a1 */
    jal ra, kernel_init         /* Jump to kernel initialization */

    /* Should never return */
halt:
    wfi                         /* Wait for interrupt */
    jal zero, halt              /* Loop forever if we do return */

/* Trap handler */
.align 4
trap_vector:
    /* Save context */
    addi sp, sp, -256
    sd ra, 0(sp)
    sd sp, 8(sp)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    /* Call trap handler */
    call trap_handler

    /* Restore context */
    ld ra, 0(sp)
    ld sp, 8(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)
    addi sp, sp, 256

    sret

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
