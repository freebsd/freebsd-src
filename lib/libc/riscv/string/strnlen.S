/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <machine/asm.h>

/*
 * a0 - const char *s
 * a1 - size_t maxlen;
 */
ENTRY(strnlen)
	/*
	 * a0 - const char *s;
	 * a1 - size_t maxlen;
	 * a2 - uint64_t *ptr;
	 * a3 - char iter[8];
	 * a4 - uint64_t *end_align;
	 * a5 - uint64_t *end_unroll;
	 */

	beqz a1, .Lnot_found

	/* ptr = s & ~0b111 */
	/* t0 = 0x0101010101010101 */
	/* t1 = 0x8080808080808080 */
	/* end_align = (s + maxlen + 7) & ~0b111 */
	/* mask_start = t0 >> ((-s.value) << 3) */
	add a4, a0, a1
	li t0, 0x01010101
	addi a4, a4, 7
	slli t1, t0, 32
	neg t2, a0
	andi a4, a4, ~0b111
	or t0, t0, t1
	slli t2, t2, 3
	andi a2, a0, ~0b111
	slli t1, t0, 7
	srl t2, t0, t2

	/* if pointer is aligned skip to loop */
	beq a0, a2, .Lskip_start

	/* iter = *ptr */
	ld a3, (a2)

	/* iter = iter | mask_start */
	or a3, a3, t2

	/* has_zero */
	not t2, a3
	sub a3, a3, t0
	and t2, t2, t1
	and a3, a3, t2

	addi a2, a2, 8
	bnez a3, .Lfind_zero

.Lskip_start:
	/* end_unroll */
	sub t2, a4, a2
	andi t2, t2, ~0b1111
	add a5, a2, t2

	/* while (ptr != end_unroll) */
	beq a2, a5, .Lskip_loop
.Lloop:
	ld a3, (a2)
	ld a6, 8(a2)

	/* has_zero */
	not t2, a3
	not t3, a6
	sub a3, a3, t0
	sub a6, a6, t0
	and t2, t2, t1
	and t3, t3, t1
	and a3, a3, t2
	and a6, a6, t3

	addi a2, a2, 8
	bnez a3, .Lfind_zero

	mv a3, a6

	addi a2, a2, 8
	bnez a3, .Lfind_zero

	bne a2, a5, .Lloop

.Lskip_loop:

	beq a2, a4, .Lnot_found

	ld a3, (a2)

	/* has_zero */
	not t2, a3
	sub a3, a3, t0
	and t2, t2, t1
	and a3, a3, t2


	addi a2, a2, 8
	beqz a3, .Lnot_found

.Lfind_zero:

	/* move ptr back */
	addi a2, a2, -8

	/* isolate lowest set bit */
	neg t0, a3
	and a3, a3, t0

	li t0, 0x0001020304050607
	srli a3, a3, 7

	/* lowest set bit is 2^(8*k)
	 * multiplying by it shifts the idx array in t0 by k bytes to the left */
	mul	a3, a3, t0

	/* highest byte contains idx of first zero */
	srli a3, a3, 56

	/* zero_idx */
	sub a2, a2, a0
	add a2, a2, a3

	/* min(zero_idx, maxlen) */
	sub a2, a2, a1
	srai t1, a2, 63
	and a2, a2, t1
	add a0, a1, a2

	ret

.Lnot_found:
	mv a0, a1
	ret

END(strnlen)
