/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <machine/asm.h>

/*
 * a0 - void* dst
 * a1 - const void* src
 * a2 - size_t len
 */
ENTRY(memcpy)
	beqz a2, .Lreturn

	/* diff = (dstv - srcv) & 0b111 */
	sub t0, a0, a1
	andi t0, t0, 0b111

	sltiu t1, a2, 8

	/* we never change a0, because memcpy returns the original dst */
	mv a3, a0

	/* len < 8 */
	bnez t1, .Lend

	/* t1 = (-dst) & 0b111 */
	neg t1, a0
	andi t1, t1, 0b111

	sub a2, a2, t1

	la t2, .Lduff_start
	slli t3, t1, 3
	sub t2, t2, t3
	jr t2
	lb t3, 6(a1)
	sb t3, 6(a3)
	lb t3, 5(a1)
	sb t3, 5(a3)
	lb t3, 4(a1)
	sb t3, 4(a3)
	lb t3, 3(a1)
	sb t3, 3(a3)
	lb t3, 2(a1)
	sb t3, 2(a3)
	lb t3, 1(a1)
	sb t3, 1(a3)
	lb t3, 0(a1)
	sb t3, 0(a3)
.Lduff_start:

	add a1, a1, t1
	add a3, a3, t1

	beqz a2, .Lreturn

	beqz t0, .Lmemcpy8

	/*
	 * a4 - size_t right_shift
	 * a5 - size_t left_shift
	 * a6 - size_t whole (number of dword stores)
	 */

	/* right_shift = (src % 0b111) * 8; */
	andi a4, a1, 0b111
	slli a4, a4, 3

	/* left_shift = 64 - right_shift */
	neg a5, a4

	/* whole = len / 8 */
	srli a6, a2, 3

	/* len = len % 8 */
	andi a2, a2, 0b111

	/* t0 - uint64_t* ptr */

	/* ptr = src & ~0b111 */
	andi t0, a1, ~0b111

	/* src += whole * 8 */
	slli t1, a6, 3
	add a1, a1, t1

	/*
	 * t1 - uint64_t low
	 * t2 - uint64_t high
	 */

	/* low = *ptr++ */
	ld t1, (t0)
	addi t0, t0, 8

	/* low >>= right_shift */
	srl t1, t1, a4

	beqz a6, .Llmain_skip
.Llmain:
	/* high = *ptr++ */
	ld t2, (t0)
	addi t0, t0, 8

	/* whole-- */
	addi a6, a6, -1

	/* temp = (high << left_shift) | low */
	sll t3, t2, a5
	or t3, t3, t1

	/* low = high >> right_shift */
	srl t1, t2, a4

	/* *dst++ = temp */
	sd t3, (a3)
	addi a3, a3, 8

	bnez a6, .Llmain

.Llmain_skip:

.Lend:
	la t1, .Lduff_end
	slli t2, a2, 3
	sub t1, t1, t2
	jr t1
	lb t2, 6(a1)
	sb t2, 6(a3)
	lb t2, 5(a1)
	sb t2, 5(a3)
	lb t2, 4(a1)
	sb t2, 4(a3)
	lb t2, 3(a1)
	sb t2, 3(a3)
	lb t2, 2(a1)
	sb t2, 2(a3)
	lb t2, 1(a1)
	sb t2, 1(a3)
	lb t2, 0(a1)
	sb t2, 0(a3)
.Lduff_end:

.Lreturn:
	ret

/* exectued when dst - src is multiple of 8
 * a0 - void* dst
 * a1 - const void* src
 * a2 - size_t len
 */
.Lmemcpy8:

	beqz a2, .Lreturn

	slti t0, a2, 128
	bnez t0, .Llmain8_64_skip

	/* a4 - uint64_t* end_unroll */

	/* end_unroll = dst + len / 64 * 64 */
	andi t0, a2, ~0b111111
	add a4, a3, t0

	/* len = len % 64 */
	andi a2, a2, 0b111111

.Llmain8_64:
	ld t0, 0(a1)
	ld t1, 8(a1)
	ld t2, 16(a1)
	ld t3, 24(a1)
	sd t0, 0(a3)
	sd t1, 8(a3)
	sd t2, 16(a3)
	sd t3, 24(a3)
	ld t0, 32(a1)
	ld t1, 40(a1)
	ld t2, 48(a1)
	ld t3, 56(a1)
	sd t0, 32(a3)
	sd t1, 40(a3)
	sd t2, 48(a3)
	sd t3, 56(a3)
	addi a3, a3, 64
	addi a1, a1, 64
	bne a3, a4, .Llmain8_64
.Llmain8_64_skip:

	beqz a2, .Lreturn

	/* a4 - uint64_t* end_align */

	/* end_align = (dst + len) & ~0b111 */
	add a4, a3, a2
	andi a4, a4, ~0b111

	/* len = len % 8 */
	andi a2, a2, 0b111

	beq a3, a4, .Llmain8_skip
.Llmain8:
	ld t0, (a1)
	sd t0, (a3)
	addi a3, a3, 8
	addi a1, a1, 8
	bne a3, a4, .Llmain8
.Llmain8_skip:

	la t1, .Lduff_end
	slli t2, a2, 3
	sub t1, t1, t2
	jr t1
END(memcpy)
