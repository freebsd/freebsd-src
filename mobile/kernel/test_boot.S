/*
 * Minimal RISC-V test kernel — prints over UART and halts.
 * No C code, no subsystems, no BSS. Pure assembly.
 *
 * QEMU virt UART: ns16550a at 0x1000_0000
 * OpenSBI puts us in S-mode with a0=hartid, a1=dtb.
 */

    .section ".text.entry"
    .global _start
    .align 4

_start:
    /* Save boot args */
    mv      s0, a0                  /* hartid */
    mv      s1, a1                  /* dtb */

    /* Simple stack: 4 KB in .bss */
    la      sp, _stack_top
    li      t0, 0x1000
    sub     sp, sp, t0

    /* Only hart 0 runs */
    beqz    s0, .Lrun
.Lpark:
    wfi
    j       .Lpark

.Lrun:
    /* Print banner */
    li      a0, 0x0A                /* '\n' */
    call    uart_putc
    li      a0, 0x0D                /* '\r' */
    call    uart_putc

    la      t0, .Lhello
1:  lbu     a0, 0(t0)
    beqz    a0, 1f
    call    uart_putc
    addi    t0, t0, 1
    j       1b
1:

    /* Print hartid */
    li      a0, 0x0A
    call    uart_putc
    li      a0, 'H'
    call    uart_putc
    li      a0, 'a'
    call    uart_putc
    li      a0, 'r'
    call    uart_putc
    li      a0, 't'
    call    uart_putc
    li      a0, ':'
    call    uart_putc

    mv      a0, s0
    li      t0, 10
    blt     s0, t0, .Ldigit
    /* hartid >= 10: print tens */
    li      a0, '1'
    call    uart_putc
.Ldigit:
    addi    a0, s0, '0'
    call    uart_putc

    li      a0, 0x0A
    call    uart_putc
    li      a0, 0x0D
    call    uart_putc

    la      t0, .Lboot_ok
2:  lbu     a0, 0(t0)
    beqz    a0, 2f
    call    uart_putc
    addi    t0, t0, 1
    j       2b
2:

    /* Halt */
3:  wfi
    j       3b

.Lhello:
    .string "HELLO FROM UOS(m) RISC-V BOOT\r\n"
.Lboot_ok:
    .string "[OK] Kernel entered S-mode successfully. Halting.\r\n"

    .section ".bss"
    .global _stack_top
_stack_top:
    .space  0x1000                  /* 4 KB stack */
