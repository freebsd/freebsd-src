/*-
 * Copyright (c) 2002 Peter Grehan.
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
/*      $NetBSD: sigsetjmp.S,v 1.4 1998/10/03 12:30:38 tsubai Exp $     */

#include <machine/asm.h>
/*
 * C library -- sigsetjmp, siglongjmp
 *
 *      siglongjmp(a,v)
 * will generate a "return(v?v:1)" from the last call to
 *      sigsetjmp(a, savemask)
 * by restoring registers from the stack.
 * The previous signal state is restored if savemask is non-zero
 *
 * jmpbuf layout:
 *     +------------+
 *     |  savemask  |
 *     +------------+
 *     | sig state  |
 *     |            |
 *     | (4 words)  |
 *     |            |
 *     +------------+
 *     | saved regs |
 *     |    ...     |
 */

	
#include <sys/syscall.h>

#ifdef IN_RTLD
/*
 * rtld can't use ifuncs, so it builds this file itself and gets a sigsetjmp
 * that checks for Altivec when it's called.
 */
ENTRY(sigsetjmp)
	addis	%r5,%r2,cpu_features@toc@ha
	ld	%r5,cpu_features@toc@l(%r5)
	andis.	%r5,%r5,0x1000		/* PPC_FEATURE_HAS_ALTIVEC */
	beq	.Lplain
	b	.Laltivec
END(sigsetjmp)
#endif

/*
 * v20-v31 are non-volatile too, but not every CPU has them, so an ifunc
 * picks this entry when they're there. stvx needs 16-byte alignment, which
 * jmp_buf doesn't have, so the save area is rounded up and its offset is
 * left for longjmp.
 */
ENTRY(__sigsetjmp_altivec)
.Laltivec:
	addi	%r5,%r3,40 + 42*8 + 15
	clrrdi	%r5,%r5,4
	subf	%r0,%r3,%r5
	std	%r0,40 + 41*8(%r3)
	stvx	%v20,0,%r5
	addi	%r5,%r5,16
	stvx	%v21,0,%r5
	addi	%r5,%r5,16
	stvx	%v22,0,%r5
	addi	%r5,%r5,16
	stvx	%v23,0,%r5
	addi	%r5,%r5,16
	stvx	%v24,0,%r5
	addi	%r5,%r5,16
	stvx	%v25,0,%r5
	addi	%r5,%r5,16
	stvx	%v26,0,%r5
	addi	%r5,%r5,16
	stvx	%v27,0,%r5
	addi	%r5,%r5,16
	stvx	%v28,0,%r5
	addi	%r5,%r5,16
	stvx	%v29,0,%r5
	addi	%r5,%r5,16
	stvx	%v30,0,%r5
	addi	%r5,%r5,16
	stvx	%v31,0,%r5
	b	.Lsave
END(__sigsetjmp_altivec)

ENTRY(__sigsetjmp)
.Lplain:
	li	%r0,0			/* no v20-v31 */
	std	%r0,40 + 41*8(%r3)
.Lsave:
	mr	%r6,%r3
	stw	%r4,0(%r3)
	or.	%r7,%r4,%r4
	beq	1f
	li	%r3,1			/* SIG_BLOCK, but doesn't matter */
					/*            since set == NULL  */
	li	%r4,0			/* set = NULL */
	mr	%r5,%r6			/* &oset */
	addi	%r5,%r5,4
	li	%r0, SYS_sigprocmask   /* sigprocmask(SIG_BLOCK, NULL, &oset)*/
	sc				/* assume no error       XXX */
1:
	mflr	%r11
	mfcr	%r12
	mr	%r10,%r1
	mr	%r9,%r2

	std	%r9,40 + 0*8(%r6)
	stfd	%f14,40 + 23*8(%r6)
	std	%r10,40 + 1*8(%r6)
	stfd	%f15,40 + 24*8(%r6)
	std	%r11,40 + 2*8(%r6)
	stfd	%f16,40 + 25*8(%r6)
	std	%r12,40 + 3*8(%r6)
	stfd	%f17,40 + 26*8(%r6)
	std	%r13,40 + 4*8(%r6)
	stfd	%f18,40 + 27*8(%r6)
	std	%r14,40 + 5*8(%r6)
	stfd	%f19,40 + 28*8(%r6)
	std	%r15,40 + 6*8(%r6)
	stfd	%f20,40 + 29*8(%r6)
	std	%r16,40 + 7*8(%r6)
	stfd	%f21,40 + 30*8(%r6)
	std	%r17,40 + 8*8(%r6)
	stfd	%f22,40 + 31*8(%r6)
	std	%r18,40 + 9*8(%r6)
	stfd	%f23,40 + 32*8(%r6)
	std	%r19,40 + 10*8(%r6)
	stfd	%f24,40 + 33*8(%r6)
	std	%r20,40 + 11*8(%r6)
	stfd	%f25,40 + 34*8(%r6)
	std	%r21,40 + 12*8(%r6)
	stfd	%f26,40 + 35*8(%r6)
	std	%r22,40 + 13*8(%r6)
	stfd	%f27,40 + 36*8(%r6)
	std	%r23,40 + 14*8(%r6)
	stfd	%f28,40 + 37*8(%r6)
	std	%r24,40 + 15*8(%r6)
	stfd	%f29,40 + 38*8(%r6)
	std	%r25,40 + 16*8(%r6)
	stfd	%f30,40 + 39*8(%r6)
	std	%r26,40 + 17*8(%r6)
	stfd	%f31,40 + 40*8(%r6)
	std	%r27,40 + 18*8(%r6)
	std	%r28,40 + 19*8(%r6)
	std	%r29,40 + 20*8(%r6)
	std	%r30,40 + 21*8(%r6)
	std	%r31,40 + 22*8(%r6)

	li	%r3,0
	blr
END(__sigsetjmp)

ENTRY(siglongjmp)
	ld	%r9,40 + 0*8(%r3)
	lfd	%f14,40 + 23*8(%r3)
	ld	%r10,40 + 1*8(%r3)
	lfd	%f15,40 + 24*8(%r3)
	ld	%r11,40 + 2*8(%r3)
	lfd	%f16,40 + 25*8(%r3)
	ld	%r12,40 + 3*8(%r3)
	lfd	%f17,40 + 26*8(%r3)
	ld	%r14,40 + 5*8(%r3)
	lfd	%f18,40 + 27*8(%r3)
	ld	%r15,40 + 6*8(%r3)
	lfd	%f19,40 + 28*8(%r3)
	ld	%r16,40 + 7*8(%r3)
	lfd	%f20,40 + 29*8(%r3)
	ld	%r17,40 + 8*8(%r3)
	lfd	%f21,40 + 30*8(%r3)
	ld	%r18,40 + 9*8(%r3)
	lfd	%f22,40 + 31*8(%r3)
	ld	%r19,40 + 10*8(%r3)
	lfd	%f23,40 + 32*8(%r3)
	ld	%r20,40 + 11*8(%r3)
	lfd	%f24,40 + 33*8(%r3)
	ld	%r21,40 + 12*8(%r3)
	lfd	%f25,40 + 34*8(%r3)
	ld	%r22,40 + 13*8(%r3)
	lfd	%f26,40 + 35*8(%r3)
	ld	%r23,40 + 14*8(%r3)
	lfd	%f27,40 + 36*8(%r3)
	ld	%r24,40 + 15*8(%r3)
	lfd	%f28,40 + 37*8(%r3)
	ld	%r25,40 + 16*8(%r3)
	lfd	%f29,40 + 38*8(%r3)
	ld	%r26,40 + 17*8(%r3)
	lfd	%f30,40 + 39*8(%r3)
	ld	%r27,40 + 18*8(%r3)
	lfd	%f31,40 + 40*8(%r3)
	ld	%r28,40 + 19*8(%r3)
	ld	%r29,40 + 20*8(%r3)
	ld	%r30,40 + 21*8(%r3)
	ld	%r31,40 + 22*8(%r3)
	ld	%r5,40 + 41*8(%r3)	/* offset of v20-v31, 0 if not saved */
	cmpdi	%r5,0
	beq	3f
	add	%r5,%r5,%r3
	andi.	%r0,%r5,15
	beq	2f
	/*
	 * The buffer was copied and v20-v31 aren't aligned any more, so
	 * load them through the red zone.
	 */
	addi	%r6,%r1,-192
	addi	%r5,%r5,-8
	addi	%r7,%r6,-8
	li	%r0,24
	mtctr	%r0
1:
	ldu	%r0,8(%r5)
	stdu	%r0,8(%r7)
	bdnz	1b
	mr	%r5,%r6
2:
	lvx	%v20,0,%r5
	addi	%r5,%r5,16
	lvx	%v21,0,%r5
	addi	%r5,%r5,16
	lvx	%v22,0,%r5
	addi	%r5,%r5,16
	lvx	%v23,0,%r5
	addi	%r5,%r5,16
	lvx	%v24,0,%r5
	addi	%r5,%r5,16
	lvx	%v25,0,%r5
	addi	%r5,%r5,16
	lvx	%v26,0,%r5
	addi	%r5,%r5,16
	lvx	%v27,0,%r5
	addi	%r5,%r5,16
	lvx	%v28,0,%r5
	addi	%r5,%r5,16
	lvx	%v29,0,%r5
	addi	%r5,%r5,16
	lvx	%v30,0,%r5
	addi	%r5,%r5,16
	lvx	%v31,0,%r5
3:

	lwz	%r7,0(%r3)
	mr	%r6,%r4
	mtlr	%r11
	mtcr	%r12
	mr	%r2,%r9
	mr	%r1,%r10
	or.	%r7,%r7,%r7
	beq	1f
	mr	%r4,%r3
	li	%r3,3			/* SIG_SETMASK */
	addi	%r4,%r4,4		/* &set */
	li	%r5,0			/* oset = NULL */
	li	%r0,SYS_sigprocmask	/* sigprocmask(SIG_SET, &set, NULL) */
	sc				/* assume no error       XXX */
1:
	or.	%r3,%r6,%r6
	bnelr
	li	%r3,1
	blr
END(siglongjmp)

	.section .note.GNU-stack,"",%progbits
