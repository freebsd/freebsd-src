	.file "reg_round.S"
/*
 *  reg_round.S
 *
 * Rounding/truncation/etc for FPU basic arithmetic functions.
 *
 * This code has four possible entry points.
 * The following must be entered by a jmp intruction:
 *   FPU_round, FPU_round_sqrt, and FPU_Arith_exit.
 *
 * The _round_reg entry point is intended to be used by C code.
 * From C, call as:
 * void round_reg(FPU_REG *arg, unsigned int extent, unsigned int control_w)
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
 *     $Id: reg_round.s,v 1.3 1994/06/10 07:44:55 rich Exp $
 *
 */


/*---------------------------------------------------------------------------+
 | Four entry points.                                                        |
 |                                                                           |
 | Needed by both the FPU_round and FPU_round_sqrt entry points:             |
 |  %eax:%ebx  64 bit significand                                            |
 |  %edx       32 bit extension of the significand                           |
 |  %edi       pointer to an FPU_REG for the result to be stored             |
 |  stack      calling function must have set up a C stack frame and         |
 |             pushed %esi, %edi, and %ebx                                   |
 |                                                                           |
 | Needed just for the FPU_round_sqrt entry point:                           |
 |  %cx  A control word in the same format as the FPU control word.          |
 | Otherwise, PARAM4 must give such a value.                                 |
 |                                                                           |
 |                                                                           |
 | The significand and its extension are assumed to be exact in the          |
 | following sense:                                                          |
 |   If the significand by itself is the exact result then the significand   |
 |   extension (%edx) must contain 0, otherwise the significand extension    |
 |   must be non-zero.                                                       |
 |   If the significand extension is non-zero then the significand is        |
 |   smaller than the magnitude of the correct exact result by an amount     |
 |   greater than zero and less than one ls bit of the significand.          |
 |   The significand extension is only required to have three possible       |
 |   non-zero values:                                                        |
 |       less than 0x80000000  <=> the significand is less than 1/2 an ls    |
 |                                 bit smaller than the magnitude of the     |
 |                                 true exact result.                        |
 |         exactly 0x80000000  <=> the significand is exactly 1/2 an ls bit  |
 |                                 smaller than the magnitude of the true    |
 |                                 exact result.                             |
 |    greater than 0x80000000  <=> the significand is more than 1/2 an ls    |
 |                                 bit smaller than the magnitude of the     |
 |                                 true exact result.                        |
 |                                                                           |
 +---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------+
 |  The code in this module has become quite complex, but it should handle   |
 |  all of the FPU flags which are set at this stage of the basic arithmetic |
 |  computations.                                                            |
 |  There are a few rare cases where the results are not set identically to  |
 |  a real FPU. These require a bit more thought because at this stage the   |
 |  results of the code here appear to be more consistent...                 |
 |  This may be changed in a future version.                                 |
 +---------------------------------------------------------------------------*/


#include "fpu_asm.h"
#include "exception.h"
#include "control_w.h"

#define	LOST_DOWN	$1
#define	LOST_UP		$2
#define	DENORMAL	$1
#define	UNMASKED_UNDERFLOW $2

.data
	.align 2,0
FPU_bits_lost:
	.byte	0
FPU_denormal:
	.byte	0

.text
	.align 2,144
.globl FPU_round
.globl FPU_round_sqrt
.globl FPU_Arith_exit
.globl _round_reg

/* Entry point when called from C */
_round_reg:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	PARAM1,%edi
	movl	SIGH(%edi),%eax
	movl	SIGL(%edi),%ebx
	movl	PARAM2,%edx
	movl	PARAM3,%ecx
	jmp	FPU_round_sqrt

FPU_round:		/* Normal entry point */
	movl	PARAM4,%ecx

FPU_round_sqrt:		/* Entry point from wm_sqrt.S */

#ifdef PARANOID
/* Cannot use this here yet */
/*	orl	%eax,%eax */
/*	jns	L_entry_bugged */
#endif PARANOID

	cmpl	EXP_UNDER,EXP(%edi)
	jle	xMake_denorm			/* The number is a de-normal*/

	movb	$0,FPU_denormal			/* 0 -> not a de-normal*/

xDenorm_done:
	movb	$0,FPU_bits_lost		/*No bits yet lost in rounding*/

	movl	%ecx,%esi
	andl	CW_PC,%ecx
	cmpl	PR_64_BITS,%ecx
	je	LRound_To_64

	cmpl	PR_53_BITS,%ecx
	je	LRound_To_53

	cmpl	PR_24_BITS,%ecx
	je	LRound_To_24

#ifdef PARANOID
	jmp	L_bugged	/* There is no bug, just a bad control word */
#endif PARANOID


/* Round etc to 24 bit precision */
LRound_To_24:
	movl	%esi,%ecx
	andl	CW_RC,%ecx
	cmpl	RC_RND,%ecx
	je	LRound_nearest_24

	cmpl	RC_CHOP,%ecx
	je	LCheck_truncate_24

	cmpl	RC_UP,%ecx		/* Towards +infinity */
	je	LUp_24

	cmpl	RC_DOWN,%ecx		/* Towards -infinity */
	je	LDown_24

#ifdef PARANOID
	jmp	L_bugged
#endif PARANOID

LUp_24:
	cmpb	SIGN_POS,SIGN(%edi)
	jne	LCheck_truncate_24	/* If negative then  up==truncate */

	jmp	LCheck_24_round_up

LDown_24:
	cmpb	SIGN_POS,SIGN(%edi)
	je	LCheck_truncate_24	/* If positive then  down==truncate */

LCheck_24_round_up:
	movl	%eax,%ecx
	andl	$0x000000ff,%ecx
	orl	%ebx,%ecx
	orl	%edx,%ecx
	jnz	LDo_24_round_up
	jmp	LRe_normalise

LRound_nearest_24:
	/* Do rounding of the 24th bit if needed (nearest or even) */
	movl	%eax,%ecx
	andl	$0x000000ff,%ecx
	cmpl	$0x00000080,%ecx
	jc	LCheck_truncate_24	/*less than half, no increment needed*/

	jne	LGreater_Half_24	/* greater than half, increment needed*/

	/* Possibly half, we need to check the ls bits */
	orl	%ebx,%ebx
	jnz	LGreater_Half_24	/* greater than half, increment needed*/

	orl	%edx,%edx
	jnz	LGreater_Half_24	/* greater than half, increment needed*/

	/* Exactly half, increment only if 24th bit is 1 (round to even)*/
	testl	$0x00000100,%eax
	jz	LDo_truncate_24

LGreater_Half_24:			/*Rounding: increment at the 24th bit*/
LDo_24_round_up:
	andl	$0xffffff00,%eax	/*Truncate to 24 bits*/
	xorl	%ebx,%ebx
	movb	LOST_UP,FPU_bits_lost
	addl	$0x00000100,%eax
	jmp	LCheck_Round_Overflow

LCheck_truncate_24:
	movl	%eax,%ecx
	andl	$0x000000ff,%ecx
	orl	%ebx,%ecx
	orl	%edx,%ecx
	jz	LRe_normalise			/* No truncation needed*/

LDo_truncate_24:
	andl	$0xffffff00,%eax	/* Truncate to 24 bits*/
	xorl	%ebx,%ebx
	movb	LOST_DOWN,FPU_bits_lost
	jmp	LRe_normalise


/* Round etc to 53 bit precision */
LRound_To_53:
	movl	%esi,%ecx
	andl	CW_RC,%ecx
	cmpl	RC_RND,%ecx
	je	LRound_nearest_53

	cmpl	RC_CHOP,%ecx
	je	LCheck_truncate_53

	cmpl	RC_UP,%ecx		/* Towards +infinity*/
	je	LUp_53

	cmpl	RC_DOWN,%ecx		/* Towards -infinity*/
	je	LDown_53

#ifdef PARANOID
	jmp	L_bugged
#endif PARANOID

LUp_53:
	cmpb	SIGN_POS,SIGN(%edi)
	jne	LCheck_truncate_53	/* If negative then  up==truncate*/

	jmp	LCheck_53_round_up

LDown_53:
	cmpb	SIGN_POS,SIGN(%edi)
	je	LCheck_truncate_53	/* If positive then  down==truncate*/

LCheck_53_round_up:
	movl	%ebx,%ecx
	andl	$0x000007ff,%ecx
	orl	%edx,%ecx
	jnz	LDo_53_round_up
	jmp	LRe_normalise

LRound_nearest_53:
	/*Do rounding of the 53rd bit if needed (nearest or even)*/
	movl	%ebx,%ecx
	andl	$0x000007ff,%ecx
	cmpl	$0x00000400,%ecx
	jc	LCheck_truncate_53	/* less than half, no increment needed*/

	jnz	LGreater_Half_53	/* greater than half, increment needed*/

	/*Possibly half, we need to check the ls bits*/
	orl	%edx,%edx
	jnz	LGreater_Half_53	/* greater than half, increment needed*/

	/* Exactly half, increment only if 53rd bit is 1 (round to even)*/
	testl	$0x00000800,%ebx
	jz	LTruncate_53

LGreater_Half_53:			/*Rounding: increment at the 53rd bit*/
LDo_53_round_up:
	movb	LOST_UP,FPU_bits_lost
	andl	$0xfffff800,%ebx	/* Truncate to 53 bits*/
	addl	$0x00000800,%ebx
	adcl	$0,%eax
	jmp	LCheck_Round_Overflow

LCheck_truncate_53:
	movl	%ebx,%ecx
	andl	$0x000007ff,%ecx
	orl	%edx,%ecx
	jz	LRe_normalise

LTruncate_53:
	movb	LOST_DOWN,FPU_bits_lost
	andl	$0xfffff800,%ebx	/* Truncate to 53 bits*/
	jmp	LRe_normalise


/* Round etc to 64 bit precision*/
LRound_To_64:
	movl	%esi,%ecx
	andl	CW_RC,%ecx
	cmpl	RC_RND,%ecx
	je	LRound_nearest_64

	cmpl	RC_CHOP,%ecx
	je	LCheck_truncate_64

	cmpl	RC_UP,%ecx		/* Towards +infinity*/
	je	LUp_64

	cmpl	RC_DOWN,%ecx		/* Towards -infinity*/
	je	LDown_64

#ifdef PARANOID
	jmp	L_bugged
#endif PARANOID

LUp_64:
	cmpb	SIGN_POS,SIGN(%edi)
	jne	LCheck_truncate_64	/* If negative then  up==truncate*/

	orl	%edx,%edx
	jnz	LDo_64_round_up
	jmp	LRe_normalise

LDown_64:
	cmpb	SIGN_POS,SIGN(%edi)
	je	LCheck_truncate_64	/*If positive then  down==truncate*/

	orl	%edx,%edx
	jnz	LDo_64_round_up
	jmp	LRe_normalise

LRound_nearest_64:
	cmpl	$0x80000000,%edx
	jc	LCheck_truncate_64

	jne	LDo_64_round_up

	/* Now test for round-to-even */
	testb	$1,%ebx
	jz	LCheck_truncate_64

LDo_64_round_up:
	movb	LOST_UP,FPU_bits_lost
	addl	$1,%ebx
	adcl	$0,%eax

LCheck_Round_Overflow:
	jnc	LRe_normalise		/* Rounding done, no overflow */

	/* Overflow, adjust the result (to 1.0) */
	rcrl	$1,%eax
	rcrl	$1,%ebx
	incl	EXP(%edi)
	jmp	LRe_normalise

LCheck_truncate_64:
	orl	%edx,%edx
	jz	LRe_normalise

LTruncate_64:
	movb	LOST_DOWN,FPU_bits_lost

LRe_normalise:
	testb	$0xff,FPU_denormal
	jnz	xNormalise_result

xL_Normalised:
	cmpb	LOST_UP,FPU_bits_lost
	je	xL_precision_lost_up

	cmpb	LOST_DOWN,FPU_bits_lost
	je	xL_precision_lost_down

xL_no_precision_loss:
	cmpl	EXP_OVER,EXP(%edi)
	jge	L_overflow

	/* store the result */
	movb	TW_Valid,TAG(%edi)

xL_Store_significand:
	movl	%eax,SIGH(%edi)
	movl	%ebx,SIGL(%edi)

FPU_Arith_exit:
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret


/* Set the FPU status flags to represent precision loss due to*/
/* round-up.*/
xL_precision_lost_up:
	push	%eax
	call	_set_precision_flag_up
	popl	%eax
	jmp	xL_no_precision_loss

/* Set the FPU status flags to represent precision loss due to*/
/* truncation.*/
xL_precision_lost_down:
	push	%eax
	call	_set_precision_flag_down
	popl	%eax
	jmp	xL_no_precision_loss


/* The number is a denormal (which might get rounded up to a normal)
// Shift the number right the required number of bits, which will
// have to be undone later...*/
xMake_denorm:
	/* The action to be taken depends upon whether the underflow
	// exception is masked*/
	testb	CW_Underflow,%cl		/* Underflow mask.*/
	jz	xUnmasked_underflow		/* Do not make a denormal.*/

	movb	DENORMAL,FPU_denormal

	pushl	%ecx		/* Save*/
	movl	EXP(%edi),%ecx
	subl	EXP_UNDER+1,%ecx
	negl	%ecx

	cmpl	$64,%ecx	/* shrd only works for 0..31 bits */
	jnc	xDenorm_shift_more_than_63

	cmpl	$32,%ecx	/* shrd only works for 0..31 bits */
	jnc	xDenorm_shift_more_than_32

/* We got here without jumps by assuming that the most common requirement
//   is for a small de-normalising shift.
// Shift by [1..31] bits */
	addl	%ecx,EXP(%edi)
	orl	%edx,%edx	/* extension*/
	setne	%ch
	xorl	%edx,%edx
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	orb	%ch,%dl
	popl	%ecx
	jmp	xDenorm_done

/* Shift by [32..63] bits*/
xDenorm_shift_more_than_32:
	addl	%ecx,EXP(%edi)
	subb	$32,%cl
	orl	%edx,%edx
	setne	%ch
	orb	%ch,%bl
	xorl	%edx,%edx
	shrd	%cl,%ebx,%edx
	shrd	%cl,%eax,%ebx
	shr	%cl,%eax
	orl	%edx,%edx		/*test these 32 bits*/
	setne	%cl
	orb	%ch,%bl
	orb	%cl,%bl
	movl	%ebx,%edx
	movl	%eax,%ebx
	xorl	%eax,%eax
	popl	%ecx
	jmp	xDenorm_done

/* Shift by [64..) bits*/
xDenorm_shift_more_than_63:
	cmpl	$64,%ecx
	jne	xDenorm_shift_more_than_64

/* Exactly 64 bit shift*/
	addl	%ecx,EXP(%edi)
	xorl	%ecx,%ecx
	orl	%edx,%edx
	setne	%cl
	orl	%ebx,%ebx
	setne	%ch
	orb	%ch,%cl
	orb	%cl,%al
	movl	%eax,%edx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	popl	%ecx
	jmp	xDenorm_done

xDenorm_shift_more_than_64:
	movl	EXP_UNDER+1,EXP(%edi)
/* This is easy, %eax must be non-zero, so..*/
	movl	$1,%edx
	xorl	%eax,%eax
	xorl	%ebx,%ebx
	popl	%ecx
	jmp	xDenorm_done


xUnmasked_underflow:
	/* Increase the exponent by the magic number*/
	addl	$(3*(1<<13)),EXP(%edi)
	movb	UNMASKED_UNDERFLOW,FPU_denormal
	jmp	xDenorm_done


/* Undo the de-normalisation.*/
xNormalise_result:
	cmpb	UNMASKED_UNDERFLOW,FPU_denormal
	je	xSignal_underflow

/* The number must be a denormal if we got here.*/
#ifdef PARANOID
	/* But check it... just in case.*/
	cmpl	EXP_UNDER+1,EXP(%edi)
	jne	L_norm_bugged
#endif PARANOID

	orl	%eax,%eax	/* ms bits*/
	jnz	LNormalise_shift_up_to_31	/* Shift left 0 - 31 bits*/

	orl	%ebx,%ebx
	jz	L_underflow_to_zero	/* The contents are zero*/

/* Shift left 32 - 63 bits*/
	movl	%ebx,%eax
	xorl	%ebx,%ebx
	subl	$32,EXP(%edi)

LNormalise_shift_up_to_31:
	bsrl	%eax,%ecx	/* get the required shift in %ecx */
	subl	$31,%ecx
	negl	%ecx
	shld	%cl,%ebx,%eax
	shl	%cl,%ebx
	subl	%ecx,EXP(%edi)

LNormalise_shift_done:
	testb	$0xff,FPU_bits_lost	/* bits lost == underflow*/
	jz	xL_Normalised

	/* There must be a masked underflow*/
	push	%eax
	pushl	EX_Underflow
	call	_exception
	popl	%eax
	popl	%eax
	jmp	xL_Normalised


/* The operations resulted in a number too small to represent.
// Masked response.*/
L_underflow_to_zero:
	push	%eax
	call	_set_precision_flag_down
	popl	%eax

	push	%eax
	pushl	EX_Underflow
	call	_exception
	popl	%eax
	popl	%eax

	movb	TW_Zero,TAG(%edi)
	jmp	xL_Store_significand


/* The operations resulted in a number too large to represent.*/
L_overflow:
	push	%edi
	call	_arith_overflow
	pop	%edi
	jmp	FPU_Arith_exit


xSignal_underflow:
	push	%eax
	pushl	EX_Underflow
	call	EXCEPTION
	popl	%eax
	popl	%eax
	jmp	xL_Normalised


#ifdef PARANOID
/* If we ever get here then we have problems! */
L_bugged:
	pushl	EX_INTERNAL|0x201
	call	EXCEPTION
	popl	%ebx
	jmp	FPU_Arith_exit

L_norm_bugged:
	pushl	EX_INTERNAL|0x216
	call	EXCEPTION
	popl	%ebx
	jmp	FPU_Arith_exit

L_entry_bugged:
	pushl	EX_INTERNAL|0x217
	call	EXCEPTION
	popl	%ebx
	jmp	FPU_Arith_exit
#endif PARANOID
