	.file	"reg_div.S"
/*
 *  reg_div.S
 *
 * Divide one FPU_REG by another and put the result in a destination FPU_REG.
 *
 * Call from C as:
 *   void reg_div(FPU_REG *a, FPU_REG *b, FPU_REG *dest,
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
 *                                    unsigned int control_word)
 */

#include "exception.h"
#include "fpu_asm.h"
#include "control_w.h"

.text
	.align 2

.globl	_reg_div
_reg_div:
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi
	movl	PARAM2,%ebx
	movl	PARAM3,%edi

	movb	TAG(%esi),%al
	orb	TAG(%ebx),%al

	jne	L_div_special		/* Not (both numbers TW_Valid) */

#ifdef DENORM_OPERAND
/* Check for denormals */
	cmpl	EXP_UNDER,EXP(%esi)
	jg	xL_arg1_not_denormal

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xL_arg1_not_denormal:
	cmpl	EXP_UNDER,EXP(%ebx)
	jg	xL_arg2_not_denormal

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xL_arg2_not_denormal:
#endif DENORM_OPERAND

/* Both arguments are TW_Valid */
	movb	TW_Valid,TAG(%edi)

	movb	SIGN(%esi),%cl
	cmpb	%cl,SIGN(%ebx)
	setne	(%edi)	   /* Set the sign, requires SIGN_NEG=1, SIGN_POS=0 */

	movl	EXP(%esi),%edx
	movl	EXP(%ebx),%eax
	subl	%eax,%edx
	addl	EXP_BIAS,%edx
	movl	%edx,EXP(%edi)

	jmp	_divide_kernel


/*-----------------------------------------------------------------------*/
L_div_special:
	cmpb	TW_NaN,TAG(%esi)	/* A NaN with anything to give NaN */
	je	L_arg1_NaN

	cmpb	TW_NaN,TAG(%ebx)	/* A NaN with anything to give NaN */
	jne	L_no_NaN_arg

/*  Operations on NaNs */
L_arg1_NaN:
L_arg2_NaN:
	pushl	%edi			/* Destination */
	pushl	%ebx
	pushl	%esi
	call	_real_2op_NaN
	jmp	LDiv_exit

/* Invalid operations */
L_zero_zero:
L_inf_inf:
	pushl	%edi			/* Destination */
	call	_arith_invalid		/* 0/0 or Infinity/Infinity */
	jmp	LDiv_exit

L_no_NaN_arg:
	cmpb	TW_Infinity,TAG(%esi)
	jne	L_arg1_not_inf

	cmpb	TW_Infinity,TAG(%ebx)
	je	L_inf_inf		/* invalid operation */

	cmpb	TW_Valid,TAG(%ebx)
	je	L_inf_valid

#ifdef PARANOID
	/* arg2 must be zero or valid */
	cmpb	TW_Zero,TAG(%ebx)
	ja	L_unknown_tags
#endif PARANOID

	/* Note that p16-9 says that infinity/0 returns infinity */
	jmp	L_copy_arg1		/* Answer is Inf */

L_inf_valid:
#ifdef DENORM_OPERAND
	cmpl	EXP_UNDER,EXP(%ebx)
	jg	L_copy_arg1		/* Answer is Inf */

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit
#endif DENORM_OPERAND

	jmp	L_copy_arg1		/* Answer is Inf */

L_arg1_not_inf:
	cmpb	TW_Zero,TAG(%ebx)	/* Priority to div-by-zero error */
	jne	L_arg2_not_zero

	cmpb	TW_Zero,TAG(%esi)
	je	L_zero_zero		/* invalid operation */

#ifdef PARANOID
	/* arg1 must be valid */
	cmpb	TW_Valid,TAG(%esi)
	ja	L_unknown_tags
#endif PARANOID

/* Division by zero error */
	pushl	%edi			/* destination */
	movb	SIGN(%esi),%al
	xorb	SIGN(%ebx),%al
	pushl	%eax			/* lower 8 bits have the sign */
	call	_divide_by_zero
	jmp	LDiv_exit

L_arg2_not_zero:
	cmpb	TW_Infinity,TAG(%ebx)
	jne	L_arg2_not_inf

#ifdef DENORM_OPERAND
	cmpb	TW_Valid,TAG(%esi)
	jne	L_return_zero

	cmpl	EXP_UNDER,EXP(%esi)
	jg	L_return_zero		/* Answer is zero */

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit
#endif DENORM_OPERAND

	jmp	L_return_zero		/* Answer is zero */

L_arg2_not_inf:

#ifdef PARANOID
	cmpb	TW_Zero,TAG(%esi)
	jne	L_unknown_tags
#endif PARANOID

	/* arg1 is zero, arg2 is not Infinity or a NaN */

#ifdef DENORM_OPERAND
	cmpl	EXP_UNDER,EXP(%ebx)
	jg	L_copy_arg1		/* Answer is zero */

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit
#endif DENORM_OPERAND

L_copy_arg1:
	movb	TAG(%esi),%ax
	movb	%ax,TAG(%edi)
	movl	EXP(%esi),%eax
	movl	%eax,EXP(%edi)
	movl	SIGL(%esi),%eax
	movl	%eax,SIGL(%edi)
	movl	SIGH(%esi),%eax
	movl	%eax,SIGH(%edi)

	movb	SIGN(%esi),%cl
	cmpb	%cl,SIGN(%ebx)
	jne	LDiv_negative_result

	movb	SIGN_POS,SIGN(%edi)
	jmp	LDiv_exit

LDiv_set_result_sign:
	movb	SIGN(%esi),%cl
	cmpb	%cl,SIGN(%edi)
	jne	LDiv_negative_result

	movb	SIGN_POS,SIGN(%ebx)
	jmp	LDiv_exit

LDiv_negative_result:
	movb	SIGN_NEG,SIGN(%edi)

LDiv_exit:
	leal	-12(%ebp),%esp

	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret


L_return_zero:
	movb	TW_Zero,TAG(%edi)
	jmp	LDiv_set_result_sign

#ifdef PARANOID
L_unknown_tags:
	push	EX_INTERNAL | 0x208
	call	EXCEPTION

	/* Generate a NaN for unknown tags */
	movl	_CONST_QNaN,%eax
	movl	%eax,(%edi)
	movl	_CONST_QNaN+4,%eax
	movl	%eax,SIGL(%edi)
	movl	_CONST_QNaN+8,%eax
	movl	%eax,SIGH(%edi)
	jmp	LDiv_exit
#endif PARANOID
