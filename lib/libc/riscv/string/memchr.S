/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <machine/asm.h>

/*
 * a0 - const void *b
 * a1 - int c
 * a2 - size_t len
 */
ENTRY(memchr)
	/*
	 * a0 - const char *ptr
	 * a1 - char cccccccc[8]
	 * a2 - char iter[8]
	 * a3 - uint8_t *end
	 * a4 - uint64_t *end_align
	 * a5 - uint64_t *end_unroll
	 */

	beqz a2, .Lno_match

	/* c = (uint8_t) c */
	andi a1, a1, 0xFF

	/*
	 * t0 = 0x0101010101010101
	 * t1 = 0x8080808080808080
	 * t2 = b << 3
	 * cccccccc = (uint8_t)c * t0
	 * end = b + len;
	 * ptr = b & ~0b111
	 */
	add a3, a0, a2
	li t0, 0x01010101
	sltu t2, a0, a3
	slli t1, t0, 32
	neg t2, t2
	or t0, t0, t1
	and a3, a3, t2
	slli t1, t0, 7
	slli t2, a0, 3
	and a0, a0, ~0b111
	mul a1, t0, a1

	ld a2, (a0)

	/*
	 * mask_start = REP8_0x01 ^ (REP8_0x01 << t2)
	 * iter = iter ^ cccccccc
	 * iter = iter | mask_start
	 */
	sll t2, t0, t2
	xor a2, a2, a1
	xor t2, t2, t0
	or a2, a2, t2

	/* has_zero(iter)
	 * end_align = (end + 7) & ~0b111;
	 */
	addi a4, a3, 7
	not t2, a2
	sub a2, a2, t0
	and t2, t2, t1
	andi a4, a4, ~0b111
	and a2, a2, t2

	/* ptr = ptr + 8 */
	addi a0, a0, 8

	bnez a2, .Lfind_zero

	/* if(ptr == end_align) */
	beq a0, a4, .Lno_match

	/* end_unroll = end_align & ~0b1111 */
	andi a5, a4, ~0b1111

	/*
	 * Instead of branching to check if `ptr` is 16-byte aligned:
	 *   - Probe the next 8 bytes for `c`
	 *   - Align `ptr` down to the nearest 16-byte boundary
	 *
	 * If `ptr` was already 16-byte aligned, those 8 bytes will be
	 * checked again inside the unrolled loop.
	 *
	 * This removes an unpredictable branch and improves performance.
	 */

	ld a2, (a0)
	xor a2, a2, a1

	not t2, a2
	sub a2, a2, t0
	and t2, t2, t1
	and a2, a2, t2

	addi a0, a0, 8

	bnez a2, .Lfind_zero

	andi a0, a0, ~0b1111

	/* while(ptr != end_unroll) */
	beq a0, a5, .Lskip_loop
.Lloop:
	ld a2, (a0)
	ld t3, 8(a0)

	xor a2, a2, a1
	xor t3, t3, a1

	not t2, a2
	not t4, t3
	sub a2, a2, t0
	sub t3, t3, t0
	and t2, t2, t1
	and t4, t4, t1
	and a2, a2, t2
	and t3, t3, t4

	addi a0, a0, 8

	bnez a2, .Lfind_zero

	/* move into iter for find_zero */
	mv a2, t3

	addi a0, a0, 8

	bnez a2, .Lfind_zero

	bne a0, a5, .Lloop
.Lskip_loop:

	/* there might be one 8byte left */
	beq a0, a4, .Lno_match

	ld a2, (a0)
	xor a2, a2, a1

	not t2, a2
	sub a2, a2, t0
	and t2, t2, t1
	and a2, a2, t2

	addi a0, a0, 8

	beqz a2, .Lno_match

.Lfind_zero:
	/*
	 * ptr = ptr - 8
	 * t1 = 0x0001020304050607
	 * iter = iter & (-iter)
	 * iter = iter >> 7
	 * iter = iter * t1
	 * iter = iter >> 56
	 */
	li t1, 0x10203000
	neg t0, a2
	slli t1, t1, 4
	and a2, a2, t0
	addi t1, t1, 0x405
	srli a2, a2, 7
	slli t1, t1, 16
	addi a0, a0, -8
	addi t1, t1, 0x607
	mul a2, a2, t1
	srli a2, a2, 56

	/* left = end - ptr */
	sub t0, a3, a0

	/* return iter < left ? ptr + iter : NULL */
	sltu t1, a2, t0
	neg t1, t1
	add a0, a0, a2
	and a0, a0, t1
	ret

.Lno_match:
	li a0, 0
	ret
END(memchr)
