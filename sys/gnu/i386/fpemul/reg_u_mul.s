	.file	"reg_u_mul.S"
/*
 *  reg_u_mul.S
 *
 * Core multiplication routine
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

/*---------------------------------------------------------------------------+
 |   Basic multiplication routine.                                           |
 |   Does not check the resulting exponent for overflow/underflow            |
 |                                                                           |
 |   reg_u_mul(FPU_REG *a, FPU_REG *b, FPU_REG *c, unsigned int cw);         |
 |                                                                           |
 |   Internal working is at approx 128 bits.                                 |
 |   Result is rounded to nearest 53 or 64 bits, using "nearest or even".    |
 +---------------------------------------------------------------------------*/

#include "exception.h"
#include "fpu_asm.h"
#include "control_w.h"


.data
	.align 2,0
accum_0:
	.long	0
accum_1:
	.long	0


.text
	.align 2,144

.globl _reg_u_mul
_reg_u_mul:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi
	movl	PARAM2,%edi

#ifdef PARANOID
	testl	$0x80000000,SIGH(%esi)
	jz	L_bugged
	testl	$0x80000000,SIGH(%edi)
	jz	L_bugged
#endif PARANOID

#ifdef DENORM_OPERAND
	movl	EXP(%esi),%eax
	cmpl	EXP_UNDER,%eax
	jg	xOp1_not_denorm

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xOp1_not_denorm:
	movl	EXP(%edi),%eax
	cmpl	EXP_UNDER,%eax
	jg	xOp2_not_denorm

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xOp2_not_denorm:
#endif DENORM_OPERAND

	xorl	%ecx,%ecx
	xorl	%ebx,%ebx

	movl	SIGL(%esi),%eax
	mull	SIGL(%edi)
	movl	%eax,accum_0
	movl	%edx,accum_1

	movl	SIGL(%esi),%eax
	mull	SIGH(%edi)
	addl	%eax,accum_1
	adcl	%edx,%ebx
/*	adcl	$0,%ecx		*//* overflow here is not possible */

	movl	SIGH(%esi),%eax
	mull	SIGL(%edi)
	addl	%eax,accum_1
	adcl	%edx,%ebx
	adcl	$0,%ecx

	movl	SIGH(%esi),%eax
	mull	SIGH(%edi)
	addl	%eax,%ebx
	adcl	%edx,%ecx

	movl	EXP(%esi),%eax	/* Compute the exponent */
	addl	EXP(%edi),%eax
	subl	EXP_BIAS-1,%eax
/*  Have now finished with the sources */
	movl	PARAM3,%edi	/* Point to the destination */
	movl	%eax,EXP(%edi)

/*  Now make sure that the result is normalized */
	testl	$0x80000000,%ecx
	jnz	LResult_Normalised

	/* Normalize by shifting left one bit */
	shll	$1,accum_0
	rcll	$1,accum_1
	rcll	$1,%ebx
	rcll	$1,%ecx
	decl	EXP(%edi)

LResult_Normalised:
	movl	accum_0,%eax
	movl	accum_1,%edx
	orl	%eax,%eax
	jz	L_extent_zero

	orl	$1,%edx

L_extent_zero:
	movl	%ecx,%eax
	jmp	FPU_round


#ifdef PARANOID
L_bugged:
	pushl	EX_INTERNAL|0x205
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_exit:
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret
#endif PARANOID

