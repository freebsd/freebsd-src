/*
 *  reg_norm.s
 *
 * Normalize the value in a FPU_REG.
 *
 * Call from C as:
 *   void normalize(FPU_REG *n)
 *
 *   void normalize_nuo(FPU_REG *n)
 *
 *
 * Copyright (C) 1992, 1993  W. Metzenthen, 22 Parker St, Ormond,
 *                           Vic 3163, Australia.
 *                           E-mail apm233m@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD operating system. Any other use is not permitted
 * under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "fpu_asm.h"


.text

	.align 2,144
.globl _normalize

_normalize:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx

	movl	PARAM1,%ebx

	movl	SIGH(%ebx),%edx
	movl	SIGL(%ebx),%eax

	orl	%edx,%edx	/* ms bits */
	js	L_done		/* Already normalized */
	jnz	L_shift_1	/* Shift left 1 - 31 bits */

	orl	%eax,%eax
	jz	L_zero		/* The contents are zero */

/* L_shift_32: */
	movl	%eax,%edx
	xorl	%eax,%eax
	subl	$32,EXP(%ebx)	/* This can cause an underflow */

/* We need to shift left by 1 - 31 bits */
L_shift_1:
	bsrl	%edx,%ecx	/* get the required shift in %ecx */
	subl	$31,%ecx
	negl	%ecx
	shld	%cl,%eax,%edx
	shl	%cl,%eax
	subl	%ecx,EXP(%ebx)	/* This can cause an underflow */

	movl	%edx,SIGH(%ebx)
	movl	%eax,SIGL(%ebx)

L_done:
	cmpl	EXP_OVER,EXP(%ebx)
	jge	L_overflow

	cmpl	EXP_UNDER,EXP(%ebx)
	jle	L_underflow

L_exit:
	popl	%ebx
	leave
	ret


L_zero:
	movl	EXP_UNDER,EXP(%ebx)
	movb	TW_Zero,TAG(%ebx)
	jmp	L_exit

L_underflow:
	push	%ebx
	call	_arith_underflow
	pop	%ebx
	jmp	L_exit

L_overflow:
	push	%ebx
	call	_arith_overflow
	pop	%ebx
	jmp	L_exit



/* Normalise without reporting underflow or overflow */
	.align 2,144
.globl _normalize_nuo

_normalize_nuo:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx

	movl	PARAM1,%ebx

	movl	SIGH(%ebx),%edx
	movl	SIGL(%ebx),%eax

	orl	%edx,%edx	/* ms bits */
	js	L_exit		/* Already normalized */
	jnz	L_nuo_shift_1	/* Shift left 1 - 31 bits */

	orl	%eax,%eax
	jz	L_zero		/* The contents are zero */

/* L_nuo_shift_32: */
	movl	%eax,%edx
	xorl	%eax,%eax
	subl	$32,EXP(%ebx)	/* This can cause an underflow */

/* We need to shift left by 1 - 31 bits */
L_nuo_shift_1:
	bsrl	%edx,%ecx	/* get the required shift in %ecx */
	subl	$31,%ecx
	negl	%ecx
	shld	%cl,%eax,%edx
	shl	%cl,%eax
	subl	%ecx,EXP(%ebx)	/* This can cause an underflow */

	movl	%edx,SIGH(%ebx)
	movl	%eax,SIGL(%ebx)
	jmp	L_exit


