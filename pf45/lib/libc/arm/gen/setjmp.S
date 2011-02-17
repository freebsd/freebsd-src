/*	$NetBSD: setjmp.S,v 1.5 2003/04/05 23:08:51 bjh21 Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(setjmp)
	/* Block all signals and retrieve the old signal mask */
	stmfd	sp!, {r0, r14}
	add	r2, r0, #(25 * 4) /* oset */
	mov	r0, #0x00000001 /* SIG_BLOCK */
	mov	r1, #0 /* set */

	bl	PIC_SYM(_C_LABEL(sigprocmask), PLT)

	ldmfd	sp!, {r0, r14}

	ldr	r1, .Lsetjmp_magic
	str	r1, [r0], #4

#ifdef __SOFTFP__
	add	r0, r0, #52
#else
	/* Store fp registers */
	sfm	f4, 4, [r0], #48
	/* Store fpsr */
	rfs	r1
	str	r1, [r0], #0x0004
#endif	/* __SOFTFP__ */
	/* Store integer registers */
        stmia	r0, {r4-r14}
        mov	r0, #0x00000000
	RET

.Lsetjmp_magic:
	.word	_JB_MAGIC_SETJMP


.weak _C_LABEL(longjmp)
.set _C_LABEL(longjmp), _C_LABEL(__longjmp)
ENTRY(__longjmp)
	ldr	r2, .Lsetjmp_magic
	ldr	r3, [r0]
	teq	r2, r3
	bne	botch


	/* Set signal mask */
	stmfd	sp!, {r0, r1, r14}
	sub	sp, sp, #4	/* align the stack */

	add	r1, r0, #(25 * 4) /* Signal mask */
	mov	r0, #3 /* SIG_SETMASK */
	mov	r2, #0
	bl	PIC_SYM(_C_LABEL(sigprocmask), PLT)

	add	sp, sp, #4	/* unalign the stack */
	ldmfd	sp!, {r0, r1, r14} 

	add	r0, r0, #4
#ifdef __SOFTFP__
	add	r0, r0, #52
#else
	/* Restore fp registers */
	lfm	f4, 4, [r0], #48
	/* Restore FPSR */
	ldr	r4, [r0], #0x0004
	wfs	r4
#endif	/* __SOFTFP__ */
	/* Restore integer registers */
        ldmia	r0, {r4-r14}

	/* Validate sp and r14 */
	teq	sp, #0
	teqne	r14, #0
	beq	botch

	/* Set return value */

	mov	r0, r1
	teq	r0, #0x00000000
	moveq	r0, #0x00000001
	RET

	/* validation failed, die die die. */
botch:
	bl	PIC_SYM(_C_LABEL(longjmperror), PLT)
	bl	PIC_SYM(_C_LABEL(abort), PLT)
	b	. - 8		/* Cannot get here */
