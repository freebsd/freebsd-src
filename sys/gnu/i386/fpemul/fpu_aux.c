/*
 *  fpu_aux.c
 *
 * Code to implement some of the FPU auxiliary instructions.
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
 *     $Id: fpu_aux.c,v 1.3 1994/06/10 07:44:17 rich Exp $
 *
 */


#include "param.h"
#include "proc.h"
#include "machine/cpu.h"
#include "machine/pcb.h"

#include "fpu_emu.h"
#include "fpu_system.h"
#include "exception.h"
#include "status_w.h"



void
fclex(void)
{
	status_word &= ~(SW_Backward | SW_Summary | SW_Stack_Fault | SW_Precision |
	    SW_Underflow | SW_Overflow | SW_Zero_Div | SW_Denorm_Op |
	    SW_Invalid);
	FPU_entry_eip = ip_offset;	/* We want no net effect */
}
/* Needs to be externally visible */
void
finit()
{
	int     r;
	control_word = 0x037f;
	status_word = 0;
	top = 0;		/* We don't keep top in the status word
				 * internally. */
	for (r = 0; r < 8; r++) {
		regs[r].tag = TW_Empty;
	}
	FPU_entry_eip = ip_offset = 0;
}

static FUNC finit_table[] = {
	Un_impl, Un_impl, fclex, finit, Un_impl, Un_impl, Un_impl, Un_impl
};

void
finit_()
{
	(finit_table[FPU_rm]) ();
}


static void
fstsw_ax(void)
{

	status_word &= ~SW_Top;
	status_word |= (top & 7) << SW_Top_Shift;

	*(short *) &FPU_EAX = status_word;

}

static FUNC fstsw_table[] = {
	fstsw_ax, Un_impl, Un_impl, Un_impl, Un_impl, Un_impl, Un_impl, Un_impl
};

void
fstsw_()
{
	(fstsw_table[FPU_rm]) ();
}



static void
fnop(void)
{
}

FUNC    fp_nop_table[] = {
	fnop, Un_impl, Un_impl, Un_impl, Un_impl, Un_impl, Un_impl, Un_impl
};

void
fp_nop()
{
	(fp_nop_table[FPU_rm]) ();
}


void
fld_i_()
{
	FPU_REG *st_new_ptr;

	if (STACK_OVERFLOW) {
		stack_overflow();
		return;
	}
	/* fld st(i) */
	if (NOT_EMPTY(FPU_rm)) {
		reg_move(&st(FPU_rm), st_new_ptr);
		push();
	} else {
		if (control_word & EX_Invalid) {
			/* The masked response */
			push();
			stack_underflow();
		} else
			EXCEPTION(EX_StackUnder);
	}

}


void
fxch_i()
{
	/* fxch st(i) */
	FPU_REG t;
	register FPU_REG *sti_ptr = &st(FPU_rm);

	if (FPU_st0_tag == TW_Empty) {
		if (sti_ptr->tag == TW_Empty) {
			stack_underflow();
			stack_underflow_i(FPU_rm);
			return;
		}
		reg_move(sti_ptr, FPU_st0_ptr);
		stack_underflow_i(FPU_rm);
		return;
	}
	if (sti_ptr->tag == TW_Empty) {
		reg_move(FPU_st0_ptr, sti_ptr);
		stack_underflow();
		return;
	}
	reg_move(FPU_st0_ptr, &t);
	reg_move(sti_ptr, FPU_st0_ptr);
	reg_move(&t, sti_ptr);
}


void
ffree_()
{
	/* ffree st(i) */
	st(FPU_rm).tag = TW_Empty;
}


void
ffreep()
{
	/* ffree st(i) + pop - unofficial code */
	st(FPU_rm).tag = TW_Empty;
	pop();
}


void
fst_i_()
{
	/* fst st(i) */
	reg_move(FPU_st0_ptr, &st(FPU_rm));
}


void
fstp_i()
{
	/* fstp st(i) */
	reg_move(FPU_st0_ptr, &st(FPU_rm));
	pop();
}
