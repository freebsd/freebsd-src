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
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
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
#if 0
	RCSID("$NetBSD: strcpy.S,v 1.0 2018/05/08 13:00:49 lbianc Exp $")
#endif

ENTRY(__strcpy_arch_2_05)
	mr	%r8, %r3

/* 
 * Aligning the reading address, even if it is already aligned to avoid
 * performance degradation with strings with 8 bytes or less.
 */
.Lalignment:
	lbz	%r0,0(%r4)
	cmpdi	cr7,%r0,0
	stb	%r0,0(%r8)
	beq	cr7,.Lexit
	addi	%r4,%r4,1
	addi	%r8,%r8,1
	andi.	%r0,%r4,0x7
	bne	.Lalignment

/* Copy by double word with aligned address. */
.Lcopy_dw:
	ld	%r0,0(%r4)
	xor	%r6,%r6,%r6
	cmpb	%r5,%r0,%r6
	cmpdi	cr7,%r5,0
	bne	cr7,.Lcheck_zero
	/* Backward r8 to use stdu instruction in Lcopy_dw_loop */
	addi	%r8,%r8,-8
.Lcopy_dw_loop:
	stdu	%r0,8(%r8)
	ldu	%r0,8(%r4)
	cmpb	%r5,%r0,%r6
	cmpdi	cr7,%r5,0
	beq	cr7,.Lcopy_dw_loop

	addi	%r8,%r8,8   /* Forward r8 to use std instruction. */
#if defined(__BIG_ENDIAN__)
/* Find where the zero is located. */
.Lcheck_zero:
	rldicr.	%r5,%r0,0,7
	beq	.Lfound_on_byte_0
	rldicr.	%r7,%r0,8,7
	beq	.Lfound_on_byte_1
	rldicr.	%r7,%r0,16,7
	beq	.Lfound_on_byte_2
	rldicr.	%r7,%r0,24,7
	beq	.Lfound_on_byte_3
	andis.	%r7,%r0,0xff00
	beq	.Lfound_on_byte_4
	andis.	%r7,%r0,0xff
	beq	.Lfound_on_byte_5
	andi.	%r7,%r0,0xff00
	beq	.Lfound_on_byte_6

/* Copy the last string bytes according to the string end position. */
.Lfound_on_byte_7:
	std	%r0,0(%r8)
	b	.Lexit

.Lfound_on_byte_6:
	srdi	%r6,%r0,32
	stw	%r6,0(%r8)
	srdi	%r6,%r0,16
	sth	%r6,4(%r8)
	srdi	%r6,%r0,8
	stb	%r6,6(%r8)
	b	.Lexit

.Lfound_on_byte_5:
	srdi	%r6,%r0,32
	stw	%r6,0(%r8)
	srdi	%r6,%r0,16
	sth	%r6,4(%r8)
	b	.Lexit

.Lfound_on_byte_4:
	srdi	%r6,%r0,32
	stw	%r6,0(%r8)
	srdi	%r6,%r0,24
	stb	%r6,4(%r8)
	b	.Lexit

.Lfound_on_byte_3:
	srdi	%r6,%r0,32
	stw	%r6,0(%r8)
	b	.Lexit

.Lfound_on_byte_2:
	srdi	%r6,%r0,48
	sth	%r6,0(%r8)
	srdi	%r6,%r0,40
	stb	%r6,2(%r8)
	b	.Lexit

.Lfound_on_byte_1:
	srdi	%r6,%r0,48
	sth	%r6,0(%r8)
	b	.Lexit

.Lfound_on_byte_0:
	srdi	%r6,%r0,56
	stb	%r6,0(%r8)
#elif defined(__LITTLE_ENDIAN__)
/* Find where the zero is located. */
.Lcheck_zero:
	andi.	%r7,%r0,0xff
	beq	.Lfound_on_byte_0
	andi.	%r7,%r0,0xff00
	beq	.Lfound_on_byte_1
	andis.	%r7,%r0,0xff
	beq	.Lfound_on_byte_2
	andis.	%r7,%r0,0xff00
	beq	.Lfound_on_byte_3
	rldicr.	%r7,%r0,24,7
	beq	.Lfound_on_byte_4
	rldicr.	%r7,%r0,16,7
	beq	.Lfound_on_byte_5
	rldicr.	%r7,%r0,8,7
	beq	.Lfound_on_byte_6

/* Copy the last string bytes according to the string end position. */
.Lfound_on_byte_7:
	std	%r0,0(%r8)
	b	.Lexit

.Lfound_on_byte_6:
	stw	%r0,0(%r8)
	srdi	%r6,%r0,32
	sth	%r6,4(%r8)
	srdi	%r6,%r0,48
	stb	%r6,6(%r8)
	b	.Lexit

.Lfound_on_byte_5:
	stw	%r0,0(%r8)
	srdi	%r6,%r0,32
	sth	%r6,4(%r8)
	b	.Lexit

.Lfound_on_byte_4:
	stw	%r0,0(%r8)
	srdi	%r6,%r0,32
	stb	%r6,4(%r8)
	b	.Lexit

.Lfound_on_byte_3:
	stw	%r0,0(%r8)
	b	.Lexit

.Lfound_on_byte_2:
	sth	%r0,0(%r8)
	srdi	%r6,%r0,16
	stb	%r6,2(%r8)
	b	.Lexit

.Lfound_on_byte_1:
	sth	%r0,0(%r8)
	b	.Lexit

.Lfound_on_byte_0:
	stb	%r0,0(%r8)
#else
#error "Unable to determine Endianness"
#endif
.Lexit:
	blr

END(__strcpy_arch_2_05)

	.section .note.GNU-stack,"",%progbits
