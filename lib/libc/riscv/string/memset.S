/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <machine/asm.h>

/*
 * register a0 - void *dest
 * register a1 - int c
 * register a2 - size_t len
 */
ENTRY(memset)
	andi a1, a1, 0xFF

	sltiu t1, a2, 8
	mv t0, a0
	bnez t1, .Lend

	li t1, 0x0101010101010101
	mul a1, a1, t1

	andi t1, a0, 0b111
	andi t0, a0, ~0b111

	beqz t1, .Lloop_store_64

	la t2, .Lduff_start
	slli t3, t1, 2
	add t2, t2, t3
	jr -4(t2)
.Lduff_start:
	sb a1, 1(t0)
	sb a1, 2(t0)
	sb a1, 3(t0)
	sb a1, 4(t0)
	sb a1, 5(t0)
	sb a1, 6(t0)
	sb a1, 7(t0)

	/* a3 = a3 -(8-a) <=> a3 = a3 + (a-8) */
	addi t1, t1, -8
	add a2, a2, t1
	addi t0, t0, 8

.Lloop_store_64:
	slti t1, a2, 64
	bnez t1, .Lstore_rest
	sd a1, 0(t0)
	sd a1, 8(t0)
	sd a1, 16(t0)
	sd a1, 24(t0)
	sd a1, 32(t0)
	sd a1, 40(t0)
	sd a1, 48(t0)
	sd a1, 56(t0)
	addi a2, a2, -64
	addi t0, t0, 64
	j .Lloop_store_64

.Lstore_rest:
	la t2, .Lduff_rest
	andi t3, a2, ~0b111
	srli t4, t3, 1
	sub t2, t2, t4
	jr t2
	sd a1, 56(t0)
	sd a1, 48(t0)
	sd a1, 40(t0)
	sd a1, 32(t0)
	sd a1, 24(t0)
	sd a1, 16(t0)
	sd a1, 8(t0)
	sd a1, 0(t0)
.Lduff_rest:
	add t0, t0, t3
	sub a2, a2, t3

.Lend:
	slli a2, a2, 2
	la t2, .Lduff_end
	sub t2, t2, a2
	jr t2
	sb a1, 6(t0)
	sb a1, 5(t0)
	sb a1, 4(t0)
	sb a1, 3(t0)
	sb a1, 2(t0)
	sb a1, 1(t0)
	sb a1, (t0)
.Lduff_end:
	ret
END(memset)

