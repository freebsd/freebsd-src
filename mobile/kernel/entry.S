/*
 * RISC-V Bootstrap Entry Point
 * Mobile OS - QEMU virt / WSL nographic boot
 *
 * Single-hart boot (hart 0 only). All other harts are parked in a WFI
 * loop in machine mode by OpenSBI so we don't have to manage them here.
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

    /*
     * Delegate supervisor traps/interrupts to S-mode so the kernel
     * can use a normal trap handler (scause/stvec/sret).
     * Required bits in medeleg/mideleg:
     *   bit  9 = Supervisor External Interrupt (SEI / PLIC)
     *   bit  5 = Supervisor Timer Interrupt  (STI / CLINT)
     */
    li      t0, (1 << 9) | (1 << 5)
    csrw    medeleg, t0
    csrw    mideleg, t0

    /* Set trap vector */
    la      t0, trap_vector
    csrw    stvec, t0

    /* Enable supervisor interrupts (SIE = STIE | SEIE | SSIE) */
    li      t0, (1 << 5) | (1 << 9) | (1 << 1)
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
    /* Full trap/IRQ entry for S-mode.
     * Handles: supervisor timer interrupts and supervisor external
     * interrupts (PLIC). Falls through for other cases.
     */
    addi    sp, sp, -32*8
    sd      ra, 0(sp)
    sd      sp, 8(sp)
    sd      gp, 16(sp)
    sd      tp, 24(sp)
    sd      s0, 32(sp)
    sd      s1, 40(sp)
    sd      s2, 48(sp)
    sd      s3, 56(sp)
    sd      s4, 64(sp)
    sd      s5, 72(sp)
    sd      s6, 80(sp)
    sd      s7, 88(sp)
    sd      s8, 96(sp)
    sd      s9, 104(sp)
    sd      s10, 112(sp)
    sd      s11, 120(sp)
    sd      a0, 128(sp)
    sd      a1, 136(sp)
    sd      a2, 144(sp)
    sd      a3, 152(sp)
    sd      a4, 160(sp)
    sd      a5, 168(sp)
    sd      a6, 176(sp)
    sd      a7, 184(sp)
    sd      t0, 192(sp)
    sd      t1, 200(sp)
    sd      t2, 208(sp)
    sd      t3, 216(sp)
    sd      t4, 224(sp)
    sd      t5, 232(sp)
    sd      t6, 240(sp)

    csrr    t0, scause
    srli    t0, t0, 1          /* shift away interrupt flag bit */
    li      t1, 5              /* Supervisor Timer Interrupt  */
    li      t2, 9              /* Supervisor External Interrupt */
    beq     t0, t1, handle_timer
    beq     t0, t2, handle_external
    j       restore_and_ret

handle_timer:
    /* Acknowledge by updating CLINT mtimecmp to the next deadline. */
    call    timer_ack
    call    scheduler_tick
    j       restore_and_ret

handle_external:
    /* Read IRQ number from PLIC claim register (context 0). */
    la      t3, plic_claim_base
    lw      t4, 0(t3)          /* t4 = IRQ number, 0 if none pending */
    beqz    t4, restore_and_ret

    /* Dispatch to registered handler, if any. */
    slli    t5, t4, 2
    la      t6, irq_handlers_irq
    add     t6, t6, t5
    lw      t6, 0(t6)
    beqz    t6, plic_complete

    mv      a0, t4
    la      a1, irq_priv
    add     a1, a1, t5
    lw      a1, 0(a1)
    jalr    ra, t6, 0

plic_complete:
    /* Write IRQ number back to claim/complete to signal done. */
    sw      t4, 0(t3)
    j       restore_and_ret

restore_and_ret:
    ld      ra, 0(sp)
    ld      sp, 8(sp)
    ld      gp, 16(sp)
    ld      tp, 24(sp)
    ld      s0, 32(sp)
    ld      s1, 40(sp)
    ld      s2, 48(sp)
    ld      s3, 56(sp)
    ld      s4, 64(sp)
    ld      s5, 72(sp)
    ld      s6, 80(sp)
    ld      s7, 88(sp)
    ld      s8, 96(sp)
    ld      s9, 104(sp)
    ld      s10, 112(sp)
    ld      s11, 120(sp)
    ld      a0, 128(sp)
    ld      a1, 136(sp)
    ld      a2, 144(sp)
    ld      a3, 152(sp)
    ld      a4, 160(sp)
    ld      a5, 168(sp)
    ld      a6, 176(sp)
    ld      a7, 184(sp)
    ld      t0, 192(sp)
    ld      t1, 200(sp)
    ld      t2, 208(sp)
    ld      t3, 216(sp)
    ld      t4, 224(sp)
    ld      t5, 232(sp)
    ld      t6, 240(sp)
    addi    sp, sp, 32*8
    sret

    .section ".bss"
    .global _stack_space
_stack_space:
    .space  0x10000                  /* 64 KB total (16 KB x 4 harts) */

    .global _stack_top
_stack_top:
