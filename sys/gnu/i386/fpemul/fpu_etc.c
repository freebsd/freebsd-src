/*
 *  fpu_etc.c
 *
 * Implement a few FPU instructions.
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
 *    $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <machine/cpu.h>
#include <machine/pcb.h>

#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/fpu_system.h>
#include <gnu/i386/fpemul/exception.h>
#include <gnu/i386/fpemul/status_w.h>
#include <gnu/i386/fpemul/reg_constant.h>


static void
fchs(void)
{
	if (NOT_EMPTY_0) {
		FPU_st0_ptr->sign ^= SIGN_POS ^ SIGN_NEG;
		status_word &= ~SW_C1;
	} else
		stack_underflow();
}

static void
fabs(void)
{
	if (FPU_st0_tag ^ TW_Empty) {
		FPU_st0_ptr->sign = SIGN_POS;
		status_word &= ~SW_C1;
	} else
		stack_underflow();
}


static void
ftst_(void)
{
	switch (FPU_st0_tag) {
		case TW_Zero:
		setcc(SW_C3);
		break;
	case TW_Valid:

#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		if (FPU_st0_ptr->sign == SIGN_POS)
			setcc(0);
		else
			setcc(SW_C0);
		break;
	case TW_NaN:
		setcc(SW_C0 | SW_C2 | SW_C3);	/* Operand is not comparable */
		EXCEPTION(EX_Invalid);
		break;
	case TW_Infinity:
		if (FPU_st0_ptr->sign == SIGN_POS)
			setcc(0);
		else
			setcc(SW_C0);
		EXCEPTION(EX_Invalid);
		break;
	case TW_Empty:
		setcc(SW_C0 | SW_C2 | SW_C3);
		EXCEPTION(EX_StackUnder);
		break;
	default:
		setcc(SW_C0 | SW_C2 | SW_C3);	/* Operand is not comparable */
		EXCEPTION(EX_INTERNAL | 0x14);
		break;
	}
}

static void
fxam(void)
{
	int     c = 0;
	switch (FPU_st0_tag) {
	case TW_Empty:
		c = SW_C3 | SW_C0;
		break;
	case TW_Zero:
		c = SW_C3;
		break;
	case TW_Valid:
		/* This will need to be changed if TW_Denormal is ever used. */
		if (FPU_st0_ptr->exp <= EXP_UNDER)
			c = SW_C2 | SW_C3;	/* Denormal */
		else
			c = SW_C3;
		break;
	case TW_NaN:
		c = SW_C0;
		break;
	case TW_Infinity:
		c = SW_C2 | SW_C0;
		break;
	}
	if (FPU_st0_ptr->sign == SIGN_NEG)
		c |= SW_C1;
	setcc(c);
}

static FUNC fp_etc_table[] = {
	fchs, fabs, Un_impl, Un_impl, ftst_, fxam, Un_impl, Un_impl
};

void
fp_etc()
{
	(fp_etc_table[FPU_rm]) ();
}
