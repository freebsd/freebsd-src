	.file	"reg_u_sub.S"
/*
 *  reg_u_sub.S
 *
 * Core floating point subtraction routine.
 *
 * Call from C as:
 *   void reg_u_sub(FPU_REG *arg1, FPU_REG *arg2, FPU_REG *answ,
 *                                                int control_w)
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
 *     $FreeBSD$
 *
 */

/*
 |    Kernel subtraction routine reg_u_sub(reg *arg1, reg *arg2, reg *answ).
 |    Takes two valid reg f.p. numbers (TW_Valid), which are
 |    treated as unsigned numbers,
 |    and returns their difference as a TW_Valid or TW_Zero f.p.
 |    number.
 |    The first number (arg1) must be the larger.
 |    The returned number is normalized.
 |    Basic checks are performed if PARANOID is defined.
 */

#include <gnu/i386/fpemul/exception.h>
#include <gnu/i386/fpemul/fpu_asm.h>
#include <gnu/i386/fpemul/control_w.h>

.text
	.align 2,144
.globl _reg_u_sub
_reg_u_sub:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%esi	/* source 1 */
	movl	PARAM2,%edi	/* source 2 */

#ifdef DENORM_OPERAND
	cmpl	EXP_UNDER,EXP(%esi)
	jg	xOp1_not_denorm

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xOp1_not_denorm:
	cmpl	EXP_UNDER,EXP(%edi)
	jg	xOp2_not_denorm

	call	_denormal_operand
	orl	%eax,%eax
	jnz	FPU_Arith_exit

xOp2_not_denorm:
#endif DENORM_OPERAND

/*	xorl	%ecx,%ecx */
	movl	EXP(%esi),%ecx
	subl	EXP(%edi),%ecx	/* exp1 - exp2 */

#ifdef PARANOID
	/* source 2 is always smaller than source 1 */
/*	jc	L_bugged */
	js	L_bugged_1

	testl	$0x80000000,SIGH(%edi)	/* The args are assumed to be be normalized */
	je	L_bugged_2

	testl	$0x80000000,SIGH(%esi)
	je	L_bugged_2
#endif PARANOID

/*--------------------------------------+
 |	Form a register holding the     |
 |	smaller number                  |
 +--------------------------------------*/
	movl	SIGH(%edi),%eax	/* register ms word */
	movl	SIGL(%edi),%ebx	/* register ls word */

	movl	PARAM3,%edi	/* destination */
	movl	EXP(%esi),%edx
	movl	%edx,EXP(%edi)	/* Copy exponent to destination */
	movb	SIGN(%esi),%dl
	movb	%dl,SIGN(%edi)	/* Copy the sign from the first arg */

	xorl	%edx,%edx	/* register extension */

/*--------------------------------------+
 |	Shift the temporary register	|
 |      right the required number of	|
 |	places.				|
 +--------------------------------------*/
L_shift_r:
	cmpl	$32,%ecx		/* shrd only works for 0..31 bits */
	jnc	L_more_than_31

/* less than 32 bits */
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	jmp	L_shift_done

L_more_than_31:
	cmpl	$64,%ecx
	jnc	L_more_than_63

	subb	$32,%cl
	jz	L_exactly_32

	shrd	%cl,%eax,%edx
	shr	%cl,%eax
	orl	%ebx,%ebx
	jz	L_more_31_no_low	/* none of the lowest bits is set */

	orl	$1,%edx			/* record the fact in the extension */

L_more_31_no_low:
	movl	%eax,%ebx
	xorl	%eax,%eax
	jmp	L_shift_done

L_exactly_32:
	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	jmp	L_shift_done

L_more_than_63:
	cmpw	$65,%cx
	jnc	L_more_than_64

	/* Shift right by 64 bits */
	movl	%eax,%edx
	orl	%ebx,%ebx
	jz	L_more_63_no_low

	orl	$1,%edx
	jmp	L_more_63_no_low

L_more_than_64:
	jne	L_more_than_65

	/* Shift right by 65 bits */
	/* Carry is clear if we get here */
	movl	%eax,%edx
	rcrl	%edx
	jnc	L_shift_65_nc

	orl	$1,%edx
	jmp	L_more_63_no_low

L_shift_65_nc:
	orl	%ebx,%ebx
	jz	L_more_63_no_low

	orl	$1,%edx
	jmp	L_more_63_no_low

L_more_than_65:
	movl	$1,%edx		/* The shifted nr always at least one '1' */

L_more_63_no_low:
	xorl	%ebx,%ebx
	xorl	%eax,%eax

L_shift_done:
L_subtr:
/*------------------------------+
 |	Do the subtraction	|
 +------------------------------*/
	xorl	%ecx,%ecx
	subl	%edx,%ecx
	movl	%ecx,%edx
	movl	SIGL(%esi),%ecx
	sbbl	%ebx,%ecx
	movl	%ecx,%ebx
	movl	SIGH(%esi),%ecx
	sbbl	%eax,%ecx
	movl	%ecx,%eax

#ifdef PARANOID
	/* We can never get a borrow */
	jc	L_bugged
#endif PARANOID

/*--------------------------------------+
 |	Normalize the result		|
 +--------------------------------------*/
	testl	$0x80000000,%eax
	jnz	L_round		/* no shifting needed */

	orl	%eax,%eax
	jnz	L_shift_1	/* shift left 1 - 31 bits */

	orl	%ebx,%ebx
	jnz	L_shift_32	/* shift left 32 - 63 bits */

/*	 A rare case, the only one which is non-zero if we got here
//         is:           1000000 .... 0000
//                      -0111111 .... 1111 1
//                       -------------------- 
//                       0000000 .... 0000 1  */

	cmpl	$0x80000000,%edx
	jnz	L_must_be_zero

	/* Shift left 64 bits */
	subl	$64,EXP(%edi)
	movl	%edx,%eax
	jmp	L_store

L_must_be_zero:
#ifdef PARANOID
	orl	%edx,%edx
	jnz	L_bugged_3
#endif PARANOID

	/* The result is zero */
	movb	TW_Zero,TAG(%edi)
	movl	$0,EXP(%edi)		/* exponent */
	movl	$0,SIGL(%edi)
	movl	$0,SIGH(%edi)
	jmp	L_exit		/* Does not underflow */

L_shift_32:
	movl	%ebx,%eax
	movl	%edx,%ebx
	movl	$0,%edx
	subl	$32,EXP(%edi)	/* Can get underflow here */

/* We need to shift left by 1 - 31 bits */
L_shift_1:
	bsrl	%eax,%ecx	/* get the required shift in %ecx */
	subl	$31,%ecx
	negl	%ecx
	shld	%cl,%ebx,%eax
	shld	%cl,%edx,%ebx
	shl	%cl,%edx
	subl	%ecx,EXP(%edi)	/* Can get underflow here */

L_round:
	jmp	FPU_round	/* Round the result */


#ifdef PARANOID
L_bugged_1:
	pushl	EX_INTERNAL|0x206
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_2:
	pushl	EX_INTERNAL|0x209
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_3:
	pushl	EX_INTERNAL|0x210
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged_4:
	pushl	EX_INTERNAL|0x211
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit

L_bugged:
	pushl	EX_INTERNAL|0x212
	call	EXCEPTION
	pop	%ebx
	jmp	L_exit
#endif PARANOID


L_store:
/*------------------------------+
 |	Store the result	|
 +------------------------------*/
	movl	%eax,SIGH(%edi)
	movl	%ebx,SIGL(%edi)

	movb	TW_Valid,TAG(%edi)		/* Set the tags to TW_Valid */

	cmpl	EXP_UNDER,EXP(%edi)
	jle	L_underflow

L_exit:
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret


L_underflow:
	push	%edi
	call	_arith_underflow
	pop	%ebx
	jmp	L_exit

