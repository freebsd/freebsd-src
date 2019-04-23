/*-
 * Copyright (c) 2018 Instituto de Pesquisas Eldorado
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#if 0
	RCSID("$NetBSD: strcmp.S,v 1.0 2018/05/10 12:33:02 alexandre Exp $")
#endif

/* Alignment mask. */
#define STRCMP_MULTI_ALIGNMENT_BYTES 8
#define STRCMP_MULTI_ALIGNMENT_MASK (STRCMP_MULTI_ALIGNMENT_BYTES - 1)

ENTRY(strcmp)
	/* Starting alignment even if aligned, avoiding performance
	 * degradation for short strings.
	 */
	lbz	%r5,0(%r3)		/* Load chars. */
	lbz	%r6,0(%r4)
	cmpd	%r5,%r6			/* Check if chars are different. */
	bne	.Lstrcmp_end
	cmpdi	%r5,0			/* Check if char is zero. */
	beq	.Lstrcmp_end

	/* Checking if addresses can be aligned, otherwise copy by byte */
	xor	%r7,%r3,%r4
	andi.	%r7,%r7,STRCMP_MULTI_ALIGNMENT_MASK
	bne	.Lstrcmp_compare_by_byte_loop

.Lstrcmp_param1_align_loop:
	lbzu	%r5,1(%r3)		/* Load chars. */
	lbzu	%r6,1(%r4)
	cmpd	%r5,%r6			/* Check if chars are different. */
	bne	.Lstrcmp_end
	cmpdi	%r5,0			/* Check if char is zero. */
	beq	.Lstrcmp_end
	andi.	%r7,%r3,STRCMP_MULTI_ALIGNMENT_MASK	/* Check alignment. */
	bne	.Lstrcmp_param1_align_loop

.Lstrcmp_param1_aligned:
	/* If parameter 2 is aligned compare by qword/word,
	 * else compare by byte. */
	andi.	%r7,%r4,STRCMP_MULTI_ALIGNMENT_MASK
	beq	.Lstrcmp_compare_by_word
	lbz	%r5,0(%r3)	/* Load chars. */
	lbz	%r6,0(%r4)
	cmpd	%r5,%r6		/* Check if chars are different. */
	bne+	.Lstrcmp_end
	cmpdi	%r5,0		/* Check if char is zero. */
	beq+	.Lstrcmp_end

.Lstrcmp_compare_by_byte_loop:
	lbzu	%r5,1(%r3)
	lbzu	%r6,1(%r4)
	cmpdi	%r5,0
	beq	.Lstrcmp_end
	cmpd	%r5,%r6
	beq	.Lstrcmp_compare_by_byte_loop

.Lstrcmp_end:
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_compare_by_word:
	ld	%r5,0(%r3)	/* Load double words. */
	ld	%r6,0(%r4)
	xor	%r8,%r8,%r8	/* %r8 <- Zero. */
	xor	%r0,%r5,%r6	/* Check if double words are different. */
	cmpb	%r7,%r5,%r8	/* Check if double words contain zero. */

	/*
	 * If double words are different or contain zero,
	 * find what byte is different or contains zero,
	 * else load next double words.
	 */
	or.	%r9,%r7,%r0
	bne	.Lstrcmp_check_zeros_differences

.Lstrcmp_compare_by_word_loop:
	ldu	%r5,8(%r3)	/* Load double words. */
	ldu	%r6,8(%r4)
	xor	%r0,%r5,%r6	/* Check if double words are different. */
	cmpb	%r7,%r5,%r8	/* Check if double words contain zero. */

	/*
	 * If double words are different or contain zero,
	 * find what byte is different or contains zero,
	 * else load next double words.
	 */
	or.	%r9,%r7,%r0
	beq	.Lstrcmp_compare_by_word_loop

.Lstrcmp_check_zeros_differences:
	/* Find what byte is different or contains zero. */

	/* Check 1st byte. */
	rldicr.	%r0,%r9,0,7
	bne	.Lstrcmp_found_zero_difference_in_1

	/* Check 2nd byte. */
	rldicr.	%r0,%r9,8,7
	bne	.Lstrcmp_found_zero_difference_in_2

	/* Check 3rd byte. */
	rldicr.	%r0,%r9,16,7
	bne	.Lstrcmp_found_zero_difference_in_3

	/* Check 4th byte. */
	rldicr.	%r0,%r9,24,7
	bne	.Lstrcmp_found_zero_difference_in_4

	/* Check 5th byte. */
	andis.	%r0,%r9,0xff00
	bne	.Lstrcmp_found_zero_difference_in_5

	/* Check 6th byte. */
	andis.	%r0,%r9,0xff
	bne	.Lstrcmp_found_zero_difference_in_6

	/* Check 7th byte. */
	andi.	%r0,%r9,0xff00
	bne	.Lstrcmp_found_zero_difference_in_7

.Lstrcmp_found_zero_difference_in_8:
	/* 8th byte is different or contains zero. */
	andi.	%r5,%r5,0xff
	andi.	%r6,%r6,0xff
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_1:
	/* 1st byte is different or contains zero. */
	lbz	%r5,0(%r3)
	lbz	%r6,0(%r4)
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_2:
	/* 2nd byte is different or contains zero. */
	lbz	%r5,1(%r3)
	lbz	%r6,1(%r4)
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_3:
	/* 3rd byte is different or contains zero. */
	lbz	%r5,2(%r3)
	lbz	%r6,2(%r4)
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_4:
	/* 4th byte is different or contains zero. */
	lbz	%r5,3(%r3)
	lbz	%r6,3(%r4)
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_5:
	/* 5th byte is different or contains zero. */
	lbz	%r5,4(%r3)
	lbz	%r6,4(%r4)
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_6:
	/* 6th byte is different or contains zero. */
	lbz	%r5,5(%r3)
	lbz	%r6,5(%r4)
	sub	%r3,%r5,%r6
	blr

.Lstrcmp_found_zero_difference_in_7:
	/* 7th byte is different or contains zero. */
	lbz	%r5,6(%r3)
	lbz	%r6,6(%r4)
	sub	%r3,%r5,%r6
	blr

END(strcmp)

	.section .note.GNU-stack,"",%progbits

