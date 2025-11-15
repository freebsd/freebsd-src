/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <machine/asm.h>

        .weak   strchrnul
        .set    strchrnul, __strchrnul

/*
 * a0 - const char *str
 * a1 - int c;
 */
ENTRY(__strchrnul)
	/*
	 * a0 - const char *ptr;
	 * a1 - char cccccccc[8];
	 * a2 - char iter[8];
	 * a3 - char mask_end
	 */

	/* int to char */
	andi a1, a1, 0xFF

	/* t0 = 0x0101010101010101 */
	li t0, 0x01010101
	slli t1, t0, 32
	or t0, t0, t1

	/* t1 = 0x8080808080808080 */
	slli t1, t0, 7

	/* spread char across bytes */
	mul a1, a1, t0

	/* align_offset */
	andi t2, a0, 0b111

	/* align pointer */
	andi a0, a0, ~0b111

	/* if pointer is aligned skip to loop */
	beqz t2, .Lloop

	ld a2, (a0)

	/* mask_start calculation */
	slli t2, t2, 3
	neg t2, t2
	srl t2, t0, t2

	/* fill bytes before start with non-zero */
	or a3, a2, t2

	xor a2, a2, a1
	or a2, a2, t2

	/* has_zero for \0 */
	not t3, a3
	not t2, a2
	sub a3, a3, t0
	sub a2, a2, t0
	and a3, a3, t3
	and a2, a2, t2
	and a3, a3, t1
	and a2, a2, t1


	/* if \0 or c was found, exit */
	or a2, a2, a3
	addi a0, a0, 8
	bnez a2, .Lfind_char


.Lloop:
	ld a2, (a0)

	/* has_zero for both \0 or c */
	xor a3, a2, a1

	not t2, a2
	not t3, a3
	sub a2, a2, t0
	sub a3, a3, t0
	and a2, a2, t2
	and a3, a3, t3
	and a2, a2, t1
	and a3, a3, t1

	/* if \0 or c was found, exit */
	or a2, a2, a3
	addi a0, a0, 8
	beqz a2, .Lloop

.Lfind_char:
	addi a0, a0, -8

	/* isolate lowest set bit */
	neg t0, a2
	and a2, a2, t0

	li t0, 0x0001020304050607
	srli a2, a2, 7

	/* lowest set bit is 2^(8*k)
	 * multiplying by it shifts the idx array in t0 by k bytes to the left */
	mul	a2, a2, t0

	/* highest byte contains idx of first zero */
	srli a2, a2, 56

	add a0, a0, a2
	ret
END(__strchrnul)
