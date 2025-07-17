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
 *
 */

#include <machine/asm.h>
#ifndef FN_NAME
#define FN_NAME				__memcpy
WEAK_REFERENCE(__memcpy, memcpy);
#define BLOCK_BITS			4
#endif

#define BLOCK_BYTES			(1 << BLOCK_BITS)
#define BLOCK_MASK			(BLOCK_BYTES - 1)

/* Minimum 8 byte alignment, to avoid cache-inhibited alignment faults. */
#ifndef ALIGN_MASK
#define	ALIGN_MASK			0x7
#endif

/*
 * r3: dst
 * r4: src
 * r5: len
 */
ENTRY(FN_NAME)
	cmpdi	%r5, 0			/* len == 0? nothing to do */
	beqlr-

	/* If src and dst are relatively misaligned, do byte copies. */
	andi.	%r8, %r3, ALIGN_MASK
	andi.	%r7, %r4, ALIGN_MASK
	cmpd	%r8, %r7
	mr	%r7, %r5
	mr	%r8, %r3		/* save dst */
	bne	.Lcopy_remaining_fix_index_byte

	/* align src */
.Lalignment_loop:
	lbz	%r6, 0(%r4)
	stb	%r6, 0(%r3)
	addi	%r3, %r3, 1
	addi	%r4, %r4, 1
	addi	%r5, %r5, -1
	cmpdi	%r5, 0
	beq	.Lexit
	andi.	%r0, %r4, BLOCK_MASK
	bne	.Lalignment_loop

	/* r7 = remaining, non-block, bytes */
	andi.	%r7, %r5, BLOCK_MASK

	/* Check if there are blocks of BLOCK_BYTES to be copied */
	xor.	%r5, %r5, %r7
	beq	.Lcopy_remaining_fix_index_byte

#ifdef FN_COPY_LOOP
FN_COPY_LOOP
#else
	/* Setup to copy word with ldu and stdu */
	ld	%r6, 0(%r4)
	ld	%r9, 8(%r4)
	std	%r6, 0(%r3)
	std	%r9, 8(%r3)
	addi	%r5, %r5, -BLOCK_BYTES
	cmpd	%r5, 0
	beq	.Lcopy_remaining_fix_index_word

	srdi	%r5, %r5, BLOCK_BITS
	mtctr	%r5
.Lcopy_word:
	ldu	%r6, 16(%r4)
	ld	%r9,  8(%r4)
	stdu	%r6, 16(%r3)
	std	%r9,  8(%r3)
	bdnz	.Lcopy_word

.Lcopy_remaining_fix_index_word:
	/* Check if there are remaining bytes */
	cmpd	%r7, 0
	beq	.Lexit
	addi	%r3, %r3, BLOCK_MASK
	addi	%r4, %r4, BLOCK_MASK
	b	.Lcopy_remaining
#endif

.Lcopy_remaining_fix_index_byte:
	addi	%r4, %r4, -1
	addi	%r3, %r3, -1

	/* Copy remaining bytes */
.Lcopy_remaining:
	mtctr	%r7
.Lcopy_remaining_loop:
	lbzu	%r6, 1(%r4)
	stbu	%r6, 1(%r3)
	bdnz	.Lcopy_remaining_loop

.Lexit:
	/* Restore dst */
	mr	%r3, %r8
	blr

END(FN_NAME)

	.section .note.GNU-stack,"",%progbits

