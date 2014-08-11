/*-
 * Copyright (c) 2014 Andrew Turner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Interrupt counters for vmstat.
 * TODO: These could live in a .c file.
 */
	.data
	.globl intrcnt
	.globl sintrcnt
	.globl intrnames
	.globl sintrnames

intrnames:
	.space	8
sintrnames:
	.quad	8
intrcnt:
	.space	8 * 8 /* u_long[8] */
sintrcnt:
	.quad	8

	.text
#define	EMIT(x)			\
	mov	x0, x;		\
	mov	x1, 0x1c090000; \
	str	x0, [x1];

.macro	save_registers
	stp	x28, x29, [sp, #-16]!
	stp	x26, x27, [sp, #-16]!
	stp	x24, x25, [sp, #-16]!
	stp	x22, x23, [sp, #-16]!
	stp	x20, x21, [sp, #-16]!
	stp	x18, x19, [sp, #-16]!
	stp	x16, x17, [sp, #-16]!
	stp	x14, x15, [sp, #-16]!
	stp	x12, x13, [sp, #-16]!
	stp	x10, x11, [sp, #-16]!
	stp	x8, x9, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x0, x1, [sp, #-16]!
	mrs	x10, elr_el1
	mrs	x11, spsr_el1
	mov	x12, sp
	stp	x10, x11, [sp, #-16]!
	stp	x12, lr, [sp, #-16]!
.endm

.macro	restore_registers
	ldp	x12, lr, [sp], #16
	ldp	x10, x11, [sp], #16
	mov	sp, x12
	msr	spsr_el1, x11
	msr	elr_el1, x10
	ldp	x0, x1, [sp], #16
	ldp	x2, x3, [sp], #16
	ldp	x4, x5, [sp], #16
	ldp	x6, x7, [sp], #16
	ldp	x8, x9, [sp], #16
	ldp	x10, x11, [sp], #16
	ldp	x12, x13, [sp], #16
	ldp	x14, x15, [sp], #16
	ldp	x16, x17, [sp], #16
	ldp	x18, x19, [sp], #16
	ldp	x20, x21, [sp], #16
	ldp	x22, x23, [sp], #16
	ldp	x24, x25, [sp], #16
	ldp	x26, x27, [sp], #16
	ldp	x28, x29, [sp], #16
.endm

handle_el1h_sync:
	save_registers
	mov	x0, sp
	bl	do_el1h_sync
	restore_registers
	eret

handle_el1h_irq:
	save_registers
	mov	x0, sp
	bl	cpu_intr
	restore_registers
	eret

.macro	vempty
	.align 7
	EMIT('Z');
	1: b	1b
.endm

.macro	el1h_sync
	.align 7
	b	handle_el1h_sync
.endm

.macro	el1h_irq
	.align 7
	b	handle_el1h_irq
.endm

.macro	el1h_error
	.align 7
	EMIT('C');
	1: b	1b
.endm

	.align 11
	.globl exception_vectors
exception_vectors:
	vempty		/* Synchronous EL1t */
	vempty		/* IRQ EL1t */
	vempty		/* FIQ EL1t */
	vempty		/* Error EL1t */

	el1h_sync	/* Synchronous EL1h */
	el1h_irq	/* IRQ EL1h */
	vempty		/* FIQ EL1h */
	el1h_error	/* Error EL1h */

	vempty		/* Synchronous 64-bit EL0 */
	vempty		/* IRQ 64-bit EL0 */
	vempty		/* FIQ 64-bit EL0 */
	vempty		/* Error 64-bit EL0 */

	vempty		/* Synchronous 32-bit EL0 */
	vempty		/* IRQ 32-bit EL0 */
	vempty		/* FIQ 32-bit EL0 */
	vempty		/* Error 32-bit EL0 */

