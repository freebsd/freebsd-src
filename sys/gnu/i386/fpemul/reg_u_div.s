	.file	"reg_u_div.S"
/*
 *  reg_u_div.S
 *
 * Core division routines
 *
 *
 * Copyright (C) 1992,1993,1994
 *                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,
 *                       Australia.  E-mail   billm@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD, FreeBSD and NetBSD operating systems. Any other
 * use is not permitted under this copyright.
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
 *
 * The purpose of this copyright, based upon the Berkeley copyright, is to
 * ensure that the covered software remains freely available to everyone.
 *
 * The software (with necessary differences) is also available, but under
 * the terms of the GNU copyleft, for the Linux operating system and for
 * the djgpp ms-dos extender.
 *
 * W. Metzenthen   June 1994.
 *
 *
 *      $Id: reg_u_div.s,v 1.3 1994/06/10 07:44:57 rich Exp $
 *
 */

/*---------------------------------------------------------------------------+
 |  Kernel for the division routines.                                        |
 |                                                                           |
 |  void reg_u_div(FPU_REG *a, FPU_REG *a,                                   |
 |                 FPU_REG *dest, unsigned int control_word)                 |
 |                                                                           |
 |  Does not compute the destination exponent, but does adjust it.           |
 +---------------------------------------------------------------------------*/

#include "exception.h"
#include "fpu_asm.h"
#include "control_w.h"


/* #define	dSIGL(x)	(x) */
/* #define	dSIGH(x)	4(x) */


.data
/*
	Local storage:
	Result:		accum_3:accum_2:accum_1:accum_0
	Overflow flag:	ovfl_flag
 */
	.align 2,0
accum_3:
	.long	0
accum_2:
	.long	0
accum_1:
	.long	0
accum_0:
	.long	0
result_1:
	.long	0
result_2:
	.long	0
ovfl_flag:
	.byte	0


.text
	.align 2,144

.globl _reg_u_div

.globl _divide_kernel

_reg_u_div:
	pushl	%ebp
	movl	%esp,%ebp

	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi	/* pointer to num */
	movl	PARAM2,%ebx	/* pointer to denom */
	movl	PARAM3,%edi	/* pointer to answer */

#ifdef DENORM_OPERAND
	movl	EXP(%esi),%eax
	cmpl	EXP_UNDER,%eax
	jg	xOp1_not_denorm

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xOp1_not_denorm:
	movl	EXP(%ebx),%eax
	cmpl	EXP_UNDER,%eax
	jg	xOp2_not_denorm

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xOp2_not_denorm:
#endif DENORM_OPERAND

_divide_kernel:
#ifdef PARANOID
/*	testl	$0x80000000, SIGH(%esi)	*//* Dividend */
/*	je	L_bugged */
	testl	$0x80000000, SIGH(%ebx)	/* Divisor*/
	je	L_bugged
#endif PARANOID

/* Check if the divisor can be treated as having just 32 bits */
	cmpl	$0,SIGL(%ebx)
	jnz	L_Full_Division	/* Can't do a quick divide */

/* We should be able to zip through the division here */
	movl	SIGH(%ebx),%ecx	/* The divisor */
	movl	SIGH(%esi),%edx	/* Dividend */
	movl	SIGL(%esi),%eax	/* Dividend */

	cmpl	%ecx,%edx
	setaeb	ovfl_flag	/* Keep a record */
	jb	L_no_adjust

	subl	%ecx,%edx	/* Prevent the overflow */

L_no_adjust:
	/* Divide the 64 bit number by the 32 bit denominator */
	divl	%ecx
	movl	%eax,result_2

	/* Work on the remainder of the first division */
	xorl	%eax,%eax
	divl	%ecx
	movl	%eax,result_1

	/* Work on the remainder of the 64 bit division */
	xorl	%eax,%eax
	divl	%ecx

	testb	$255,ovfl_flag	/* was the num > denom ? */
	je	L_no_overflow

	/* Do the shifting here */
	/* increase the exponent */
	incl	EXP(%edi)

	/* shift the mantissa right one bit */
	stc			/* To set the ms bit */
	rcrl	result_2
	rcrl	result_1
	rcrl	%eax

L_no_overflow:
	jmp	LRound_precision	/* Do the rounding as required*/


/*---------------------------------------------------------------------------+
 |  Divide:   Return  arg1/arg2 to arg3.                                     |
 |                                                                           |
 |  This routine does not use the exponents of arg1 and arg2, but does       |
 |  adjust the exponent of arg3.                                             |
 |                                                                           |
 |  The maximum returned value is (ignoring exponents)                       |
 |               .ffffffff ffffffff                                          |
 |               ------------------  =  1.ffffffff fffffffe                  |
 |               .80000000 00000000                                          |
 | and the minimum is                                                        |
 |               .80000000 00000000                                          |
 |               ------------------  =  .80000000 00000001   (rounded)       |
 |               .ffffffff ffffffff                                          |
 |                                                                           |
 +---------------------------------------------------------------------------*/


L_Full_Division:
	/* Save extended dividend in local register*/
	movl	SIGL(%esi),%eax
	movl	%eax,accum_2
	movl	SIGH(%esi),%eax
	movl	%eax,accum_3
	xorl	%eax,%eax
	movl	%eax,accum_1	/* zero the extension */
	movl	%eax,accum_0	/* zero the extension */

	movl	SIGL(%esi),%eax	/* Get the current num */
	movl	SIGH(%esi),%edx

/*----------------------------------------------------------------------*/
/* Initialization done */
/* Do the first 32 bits */

	movb	$0,ovfl_flag
	cmpl	SIGH(%ebx),%edx	/* Test for imminent overflow */
	jb	LLess_than_1
	ja	LGreater_than_1

	cmpl	SIGL(%ebx),%eax
	jb	LLess_than_1

LGreater_than_1:
/* The dividend is greater or equal, would cause overflow */
	setaeb	ovfl_flag		/* Keep a record */

	subl	SIGL(%ebx),%eax
	sbbl	SIGH(%ebx),%edx	/* Prevent the overflow */
	movl	%eax,accum_2
	movl	%edx,accum_3

LLess_than_1:
/* At this point, we have a dividend < divisor, with a record of
   adjustment in ovfl_flag */

	/* We will divide by a number which is too large */
	movl	SIGH(%ebx),%ecx
	addl	$1,%ecx
	jnc	LFirst_div_not_1

	/* here we need to divide by 100000000h,
	   i.e., no division at all.. */
	mov	%edx,%eax
	jmp	LFirst_div_done

LFirst_div_not_1:
	divl	%ecx		/* Divide the numerator by the augmented
				   denom ms dw */

LFirst_div_done:
	movl	%eax,result_2	/* Put the result in the answer */

	mull	SIGH(%ebx)	/* mul by the ms dw of the denom */

	subl	%eax,accum_2	/* Subtract from the num local reg */
	sbbl	%edx,accum_3

	movl	result_2,%eax	/* Get the result back */
	mull	SIGL(%ebx)	/* now mul the ls dw of the denom */

	subl	%eax,accum_1	/* Subtract from the num local reg */
	sbbl	%edx,accum_2
	sbbl	$0,accum_3
	je	LDo_2nd_32_bits		/* Must check for non-zero result here */

#ifdef PARANOID
	jb	L_bugged_1
#endif PARANOID

	/* need to subtract another once of the denom */
	incl	result_2	/* Correct the answer */

	movl	SIGL(%ebx),%eax
	movl	SIGH(%ebx),%edx
	subl	%eax,accum_1	/* Subtract from the num local reg */
	sbbl	%edx,accum_2

#ifdef PARANOID
	sbbl	$0,accum_3
	jne	L_bugged_1	/* Must check for non-zero result here */
#endif PARANOID

/*----------------------------------------------------------------------*/
/* Half of the main problem is done, there is just a reduced numerator
   to handle now */
/* Work with the second 32 bits, accum_0 not used from now on */
LDo_2nd_32_bits:
	movl	accum_2,%edx	/* get the reduced num */
	movl	accum_1,%eax

	/* need to check for possible subsequent overflow */
	cmpl	SIGH(%ebx),%edx
	jb	LDo_2nd_div
	ja	LPrevent_2nd_overflow

	cmpl	SIGL(%ebx),%eax
	jb	LDo_2nd_div

LPrevent_2nd_overflow:
/* The numerator is greater or equal, would cause overflow */
	/* prevent overflow */
	subl	SIGL(%ebx),%eax
	sbbl	SIGH(%ebx),%edx
	movl	%edx,accum_2
	movl	%eax,accum_1

	incl	result_2	/* Reflect the subtraction in the answer */

#ifdef PARANOID
	je	L_bugged_2	/* Can't bump the result to 1.0 */
#endif PARANOID

LDo_2nd_div:
	cmpl	$0,%ecx		/* augmented denom msw*/
	jnz	LSecond_div_not_1

	/* %ecx == 0, we are dividing by 1.0 */
	mov	%edx,%eax
	jmp	LSecond_div_done

LSecond_div_not_1:
	divl	%ecx		/* Divide the numerator by the denom ms dw */

LSecond_div_done:
	movl	%eax,result_1	/* Put the result in the answer */

	mull	SIGH(%ebx)	/* mul by the ms dw of the denom */

	subl	%eax,accum_1	/* Subtract from the num local reg */
	sbbl	%edx,accum_2

#ifdef PARANOID
	jc	L_bugged_2
#endif PARANOID

	movl	result_1,%eax	/* Get the result back */
	mull	SIGL(%ebx)	/* now mul the ls dw of the denom */

	subl	%eax,accum_0	/* Subtract from the num local reg */
	sbbl	%edx,accum_1	/* Subtract from the num local reg */
	sbbl	$0,accum_2

#ifdef PARANOID
	jc	L_bugged_2
#endif PARANOID

	jz	LDo_3rd_32_bits

#ifdef PARANOID
	cmpl	$1,accum_2
	jne	L_bugged_2
#endif PARANOID

	/* need to subtract another once of the denom */
	movl	SIGL(%ebx),%eax
	movl	SIGH(%ebx),%edx
	subl	%eax,accum_0	/* Subtract from the num local reg */
	sbbl	%edx,accum_1
	sbbl	$0,accum_2

#ifdef PARANOID
	jc	L_bugged_2
	jne	L_bugged_2
#endif PARANOID

	addl	$1,result_1	/* Correct the answer */
	adcl	$0,result_2

#ifdef PARANOID
	jc	L_bugged_2	/* Must check for non-zero result here */
#endif PARANOID

/*----------------------------------------------------------------------*/
/* The division is essentially finished here, we just need to perform
   tidying operations. */
/* deal with the 3rd 32 bits */
LDo_3rd_32_bits:
	movl	accum_1,%edx		/* get the reduced num */
	movl	accum_0,%eax

	/* need to check for possible subsequent overflow */
	cmpl	SIGH(%ebx),%edx	/* denom*/
	jb	LRound_prep
	ja	LPrevent_3rd_overflow

	cmpl	SIGL(%ebx),%eax	/* denom */
	jb	LRound_prep

LPrevent_3rd_overflow:
	/* prevent overflow */
	subl	SIGL(%ebx),%eax
	sbbl	SIGH(%ebx),%edx
	movl	%edx,accum_1
	movl	%eax,accum_0

	addl	$1,result_1	/* Reflect the subtraction in the answer */
	adcl	$0,result_2
	jne	LRound_prep
	jnc	LRound_prep

	/* This is a tricky spot, there is an overflow of the answer */
	movb	$255,ovfl_flag		/* Overflow -> 1.000 */

LRound_prep:
/* Prepare for rounding.
// To test for rounding, we just need to compare 2*accum with the
// denom. */
	movl	accum_0,%ecx
	movl	accum_1,%edx
	movl	%ecx,%eax
	orl	%edx,%eax
	jz	LRound_ovfl		/* The accumulator contains zero.*/

	/* Multiply by 2 */
	clc
	rcll	$1,%ecx
	rcll	$1,%edx
	jc	LRound_large		/* No need to compare, denom smaller */

	subl	SIGL(%ebx),%ecx
	sbbl	SIGH(%ebx),%edx
	jnc	LRound_not_small

	movl	$0x70000000,%eax	/* Denom was larger */
	jmp	LRound_ovfl

LRound_not_small:
	jnz	LRound_large

	movl	$0x80000000,%eax	/* Remainder was exactly 1/2 denom */
	jmp	LRound_ovfl

LRound_large:
	movl	$0xff000000,%eax	/* Denom was smaller */

LRound_ovfl:
/* We are now ready to deal with rounding, but first we must get
   the bits properly aligned */
	testb	$255,ovfl_flag	/* was the num > denom ? */
	je	LRound_precision

	incl	EXP(%edi)

	/* shift the mantissa right one bit */
	stc			/* Will set the ms bit */
	rcrl	result_2
	rcrl	result_1
	rcrl	%eax

/* Round the result as required */
LRound_precision:
	decl	EXP(%edi)	/* binary point between 1st & 2nd bits */

	movl	%eax,%edx
	movl	result_1,%ebx
	movl	result_2,%eax
	jmp	FPU_round


#ifdef PARANOID
/* The logic is wrong if we got here */
L_bugged:
	pushl	EX_INTERNAL|0x202
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_1:
	pushl	EX_INTERNAL|0x203
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_2:
	pushl	EX_INTERNAL|0x204
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
