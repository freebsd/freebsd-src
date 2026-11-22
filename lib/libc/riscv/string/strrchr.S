/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <machine/asm.h>

        .weak   rindex
        .set    rindex, strrchr

/*
 * a0 - const char *s
 * a1 - int c
 */
ENTRY(strrchr)
	/*
	 * a0 - const char *ptr_align
	 * a1 - temporary
	 * a2 -	temporary
	 * a3 - temporary
	 * a4 -	temporary
	 * a5 - const char[8] cccccccc
	 * a6 - const uint64_t *save_align
	 * a7 - const uint64_t save_iter
	 * t0 - const uintr64_t REP8_0X01
	 * t1 - const uintr64_t REP8_0X80
	 * t2 - uint64_t mask_start
	 */

	/*
	 * save_align = 0
	 * save_iter = 0xFFFFFFFFFFFFFF00
	 * REP8_0X01 = 0x0101010101010101
	 * REP8_0X80 = REP8_0X01 << 7
	 * cccccccc = (char)c * REP8_0X01
	 * ptr_align = str - str % 8
	 * a4 = str, kept to test whether str is aligned
	 * t2 = -str * 8, the shift amount for mask_start
	 */

	mv a4, a0
	li t0, 0x01010101
	li a6, 0
	neg t2, a0
	slli t1, t0, 32
	li a7, 0xFFFFFFFFFFFFFF00
	or t0, t0, t1
	slli t2, t2, 3
	andi a1, a1, 0xFF
	slli t1, t0, 7
	andi a0, a0, ~0b111
	mul a5, a1, t0

	/* if the string is aligned, skip to the loop */
	beq a4, a0, .Lloop

	/*
	 * The first word holds bytes that come before the start of the
	 * string. mask_start has 0x01 in each of those bytes. ORing it into
	 * data and iter makes them non-zero there, so they are never taken
	 * for the terminator or for a match. The rest is the loop body.
	 *
	 * mask_start = REP8_0X01 >> ((8 - str % 8) * 8)
	 */
	ld a1, 0(a0)			/* a1 -> data = *ptr_align	*/
	srl t2, t0, t2			/* mask_start = REP8_0X01 >> t2	*/
	or a1, a1, t2			/* data = data | mask_start	*/
	not a3, a1			/* a3 -> nhz = ~data		*/
	xor a2, a1, a5			/* a2 -> iter = data ^ cccccccc	*/
	or a2, a2, t2			/* iter = iter | mask_start	*/
	sub a1, a1, t0			/* a1 -> hz = data - REP8_0X01	*/
	not a4, a2			/* a4 -> nhc = ~iter		*/
	and a1, a1, a3			/* hz = hz & nhz		*/
	sub a3, a2, t0			/* a3 -> hc = iter - REP8_0X01	*/
	and a1, a1, t1			/* hz = hz & REP8_0X80		*/
	and a3, a3, a4			/* hc = hc & nhc		*/
	addi a4, a1, -1			/* a4 -> mask_end = hz - 1	*/
	and a3, a3, t1			/* hc = hc & REP8_0X80		*/
	xor a4, a4, a1			/* mask_end = mask_end ^ hz	*/
	addi a0, a0, 8			/* ptr_align = ptr_align + 8	*/
	and a3, a3, a4			/* hc = hc & mask_end		*/
	not a4, a4			/* mask_end = ~mask_end		*/

	beqz a3, .Lskip_save		/* if(!hc) goto skip_save	*/
	or a2, a2, a4			/* iter = iter | mask_end	*/
	addi a6, a0, -8			/* save_align = ptr_align - 8	*/
	mv a7, a2			/* save_iter = iter		*/

	bnez a1, .Lfind_char		/* if(hz) goto find_char	*/

.Lloop:					/* do {				*/
	ld a1, 0(a0)			/* a1 -> data = *ptr_align	*/
	not a3, a1			/* a3 -> nhz = ~data		*/
	xor a2, a1, a5			/* a2 -> iter = data ^ cccccccc	*/
	sub a1, a1, t0			/* a1 -> hz = data - REP8_0X01	*/
	not a4, a2			/* a4 -> nhc = ~iter		*/
	and a1, a1, a3			/* hz = hz & nhz		*/
	sub a3, a2, t0			/* a3 -> hc = iter - REP8_0X01	*/
	and a1, a1, t1			/* hz = hz & REP8_0X80		*/
	and a3, a3, a4			/* hc = hc & nhc		*/
	addi a4, a1, -1			/* a4 -> mask_end = hz - 1	*/
	and a3, a3, t1			/* hc = hc & REP8_0X80		*/
	xor a4, a4, a1			/* mask_end = mask_end ^ hz	*/
	addi a0, a0, 8			/* ptr_align = ptr_align + 8	*/
	and a3, a3, a4			/* hc = hc & mask_end		*/
	not a4, a4			/* mask_end = ~mask_end		*/

	beqz a3, .Lskip_save		/* if(!hc) goto skip_save	*/
	or a2, a2, a4			/* iter = iter | mask_end	*/
	addi a6, a0, -8			/* save_align = ptr_align - 8	*/
	mv a7, a2			/* save_iter = iter		*/

.Lskip_save:
	beqz a1, .Lloop			/* } while(!hz)			*/

.Lfind_char:
	/*
	 * a1 -> iter = save_iter
	 * a2 -> mask_iter = 0xFF00000000000000
	 * a3 -> match_off = 7
	 */
	li a2, 0xFF
	mv a1, a7
	slli a2, a2, 56
	li a3, 7

	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1
	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1
	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1
	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1
	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1
	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1
	and a0, a1, a2
	srli a2, a2, 8
	beqz a0, .Lret

	addi a3, a3, -1

.Lret:
	/* return save_align + match_offset */
	add a0, a6, a3
	ret
END(strrchr)