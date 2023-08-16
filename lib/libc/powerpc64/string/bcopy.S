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
#define BLOCK_SIZE_BITS			6
#define BLOCK_SIZE			(1 << BLOCK_SIZE_BITS)
#define BLOCK_SIZE_MASK			(BLOCK_SIZE - 1)

/* Minimum 8 byte alignment, to avoid cache-inhibited alignment faults.*/
#ifndef ALIGN_MASK
#define ALIGN_MASK			0x7
#endif

#define MULTI_PHASE_THRESHOLD		512

#ifndef FN_NAME
#ifdef MEMMOVE
#define FN_NAME				__memmove
WEAK_REFERENCE(__memmove, memmove);
#else
#define FN_NAME				__bcopy
WEAK_REFERENCE(__bcopy, bcopy);
#endif
#endif

/*
 * r3: dst
 * r4: src
 * r5: len
 */

ENTRY(FN_NAME)
	cmpld	%r3, %r4		/* src == dst? nothing to do */
	beqlr-
	cmpdi	%r5, 0			/* len == 0? nothing to do */
	beqlr-

#ifdef MEMMOVE
	std	%r3, -8(%r1)		/* save dst */
#else	/* bcopy: swap src/dst */
	mr	%r0, %r3
	mr	%r3, %r4
	mr	%r4, %r0
#endif

	/* First check for relative alignment, if unaligned copy one byte at a time */
	andi.	%r8, %r3, ALIGN_MASK
	andi.	%r7, %r4, ALIGN_MASK
	cmpd	%r7, %r8
	bne 	.Lunaligned


	cmpldi	%r5, MULTI_PHASE_THRESHOLD
	bge	.Lmulti_phase
	b	.Lfast_copy

.Lunaligned:
	/* forward or backward copy? */
	cmpd	%r4, %r3
	blt	.Lbackward_unaligned

	/* Just need to setup increment and jump to copy */
	li	%r0, 1
	mtctr	%r5
	b	.Lsingle_1_loop

.Lbackward_unaligned:
	/* advance src and dst to last byte, set decrement and jump to copy */
	add	%r3, %r3, %r5
	addi	%r3, %r3, -1
	add	%r4, %r4, %r5
	addi	%r4, %r4, -1
	li	%r0, -1
	mtctr	%r5
	b 	.Lsingle_1_loop

.Lfast_copy:
	/* align src */
	cmpd	%r4, %r3		/* forward or backward copy? */
	blt	.Lbackward_align

	.align 5
.Lalign:
	andi.	%r0, %r4, 15
	beq	.Lsingle_copy
	lbz	%r0, 0(%r4)
	addi	%r4, %r4, 1
	stb	%r0, 0(%r3)
	addi	%r3, %r3, 1
	addi	%r5, %r5, -1
	cmpdi	%r5, 0
	beq-	.Ldone
	b	.Lalign

.Lbackward_align:
	/* advance src and dst to end (past last byte) */
	add	%r3, %r3, %r5
	add	%r4, %r4, %r5
	.align 5
.Lbackward_align_loop:
	andi.	%r0, %r4, 15
	beq	.Lbackward_single_copy
	lbzu	%r0, -1(%r4)
	addi	%r5, %r5, -1
	stbu	%r0, -1(%r3)
	cmpdi	%r5, 0
	beq-	.Ldone
	b	.Lbackward_align_loop

.Lsingle_copy:
	/* forward copy */
	li	%r0, 1
	li	%r8, 16
	li	%r9, 0
	b	.Lsingle_phase

.Lbackward_single_copy:
	/* backward copy */
	li	%r0, -1
	li	%r8, -16
	li	%r9, -15
	/* point src and dst to last byte */
	addi	%r3, %r3, -1
	addi	%r4, %r4, -1

.Lsingle_phase:
	srdi.	%r6, %r5, 4		/* number of 16-bytes */
	beq	.Lsingle_1

	/* pre-adjustment */
	add	%r3, %r3, %r9
	add	%r4, %r4, %r9

	mtctr	%r6
	.align 5
.Lsingle_16_loop:
	ld	%r6, 0(%r4)
	ld	%r7, 8(%r4)
	add	%r4, %r4, %r8
	std	%r6, 0(%r3)
	std	%r7, 8(%r3)
	add	%r3, %r3, %r8
	bdnz	.Lsingle_16_loop

	/* post-adjustment */
	sub	%r3, %r3, %r9
	sub	%r4, %r4, %r9

.Lsingle_1:
	andi.	%r6, %r5, 0x0f		/* number of 1-bytes */
	beq	.Ldone			/* 1-bytes == 0? done */

	mtctr	%r6
	.align 5
.Lsingle_1_loop:
	lbz	%r6, 0(%r4)
	add	%r4, %r4, %r0		/* increment */
	stb	%r6, 0(%r3)
	add	%r3, %r3, %r0		/* increment */
	bdnz	.Lsingle_1_loop

.Ldone:
#ifdef MEMMOVE
	ld	%r3, -8(%r1)		/* restore dst */
#endif
	blr


.Lmulti_phase:
	/* set up multi-phase copy parameters */

	/* r7 = bytes before the aligned section of the buffer */
	andi.	%r6, %r4, 15
	subfic	%r7, %r6, 16
	/* r8 = bytes in and after the aligned section of the buffer */
	sub	%r8, %r5, %r7
	/* r9 = bytes after the aligned section of the buffer */
	andi.	%r9, %r8, BLOCK_SIZE_MASK
	/* r10 = BLOCKS in the aligned section of the buffer */
	srdi	%r10, %r8, BLOCK_SIZE_BITS

	/* forward or backward copy? */
	cmpd	%r4, %r3
	blt	.Lbackward_multi_copy

	/* set up forward copy parameters */
	std	%r7,  -32(%r1)		/* bytes to copy in phase 1 */
	std	%r10, -40(%r1)		/* BLOCKS to copy in phase 2 */
	std	%r9,  -48(%r1)		/* bytes to copy in phase 3 */

	li	%r0, 1			/* increment for phases 1 and 3 */
	li	%r5, BLOCK_SIZE		/* increment for phase 2 */

	/* op offsets for phase 2 */
	li	%r7,  0
	li	%r8,  16
	li	%r9,  32
	li	%r10, 48

	std	%r8, -16(%r1)		/* 16-byte increment (16) */
	std	%r7, -24(%r1)		/* 16-byte pre/post adjustment (0) */

	b	.Lphase1

.Lbackward_multi_copy:
	/* set up backward copy parameters */
	std	%r9,  -32(%r1)		/* bytes to copy in phase 1 */
	std	%r10, -40(%r1)		/* BLOCKS to copy in phase 2 */
	std	%r7,  -48(%r1)		/* bytes to copy in phase 3 */

	li	%r0, -1			/* increment for phases 1 and 3 */
	add	%r6, %r5, %r0		/* r6 = len - 1 */
	li	%r5, -BLOCK_SIZE	/* increment for phase 2 */
	/* advance src and dst to the last position */
	add	%r3, %r3, %r6
	add	%r4, %r4, %r6

	/* op offsets for phase 2 */
	li	%r7,  -15
	li	%r8,  -31
	li	%r9,  -47
	li	%r10, -63

	add	%r6, %r7, %r0		/* r6 = -16 */
	std	%r6, -16(%r1)		/* 16-byte increment (-16) */
	std	%r7, -24(%r1)		/* 16-byte pre/post adjustment (-15) */

.Lphase1:
	ld	%r6, -32(%r1)		/* bytes to copy in phase 1 */
	cmpldi	%r6, 0			/* r6 == 0? skip phase 1 */
	beq+	.Lphase2

	mtctr	%r6
	.align 5
.Lphase1_loop:
	lbz	%r6, 0(%r4)
	add	%r4, %r4, %r0		/* phase 1 increment */
	stb	%r6, 0(%r3)
	add	%r3, %r3, %r0		/* phase 1 increment */
	bdnz	.Lphase1_loop

.Lphase2:
	ld	%r6, -40(%r1)		/* BLOCKS to copy in phase 2 */
	cmpldi	%r6, 0			/* %r6 == 0? skip phase 2 */
	beq	.Lphase3

#ifdef FN_PHASE2
FN_PHASE2
#else
	/* save registers */
	std	%r14, -56(%r1)
	std	%r15, -64(%r1)
	std	%r16, -72(%r1)
	std	%r17, -80(%r1)
	std	%r18, -88(%r1)
	std	%r19, -96(%r1)
	std	%r20, -104(%r1)
	std	%r21, -112(%r1)

	addi	%r18, %r7, 8
	addi	%r19, %r8, 8
	addi	%r20, %r9, 8
	addi	%r21, %r10, 8

	mtctr	%r6
	.align 5
.Lphase2_loop:
	ldx	%r14, %r7,  %r4
	ldx	%r15, %r18, %r4
	ldx	%r16, %r8,  %r4
	ldx	%r17, %r19, %r4
	stdx	%r14, %r7,  %r3
	stdx	%r15, %r18, %r3
	stdx	%r16, %r8,  %r3
	stdx	%r17, %r19, %r3

	ldx	%r14, %r9,  %r4
	ldx	%r15, %r20, %r4
	ldx	%r16, %r10, %r4
	ldx	%r17, %r21, %r4
	stdx	%r14, %r9,  %r3
	stdx	%r15, %r20, %r3
	stdx	%r16, %r10, %r3
	stdx	%r17, %r21, %r3

	add	%r4, %r4, %r5		/* phase 2 increment */
	add	%r3, %r3, %r5		/* phase 2 increment */

	bdnz	.Lphase2_loop

	/* restore registers */
	ld	%r14, -56(%r1)
	ld	%r15, -64(%r1)
	ld	%r16, -72(%r1)
	ld	%r17, -80(%r1)
	ld	%r18, -88(%r1)
	ld	%r19, -96(%r1)
	ld	%r20, -104(%r1)
	ld	%r21, -112(%r1)
#endif

.Lphase3:
	/* load registers for transitioning into the single-phase logic */
	ld	%r5, -48(%r1)		/* bytes to copy in phase 3 */
	ld	%r8, -16(%r1)		/* 16-byte increment */
	ld	%r9, -24(%r1)		/* 16-byte pre/post adjustment */
	b	.Lsingle_phase

END(FN_NAME)

	.section .note.GNU-stack,"",%progbits

