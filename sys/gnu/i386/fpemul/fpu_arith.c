/*
 *  fpu_arith.c
 *
 * Code to implement the FPU register/register arithmetic instructions
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
 *     $Id: fpu_arith.c,v 1.7 1997/02/22 09:29:03 peter Exp $
 *
 */




#include <sys/param.h>
#include <sys/proc.h>
#include <machine/pcb.h>

#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/fpu_system.h>


void
fadd__()
{
	/* fadd st,st(i) */
	reg_add(FPU_st0_ptr, &st(FPU_rm), FPU_st0_ptr, control_word);
}


void
fmul__()
{
	/* fmul st,st(i) */
	reg_mul(FPU_st0_ptr, &st(FPU_rm), FPU_st0_ptr, control_word);
}



void
fsub__()
{
	/* fsub st,st(i) */
	reg_sub(FPU_st0_ptr, &st(FPU_rm), FPU_st0_ptr, control_word);
}


void
fsubr_()
{
	/* fsubr st,st(i) */
	reg_sub(&st(FPU_rm), FPU_st0_ptr, FPU_st0_ptr, control_word);
}


void
fdiv__()
{
	/* fdiv st,st(i) */
	reg_div(FPU_st0_ptr, &st(FPU_rm), FPU_st0_ptr, control_word);
}


void
fdivr_()
{
	/* fdivr st,st(i) */
	reg_div(&st(FPU_rm), FPU_st0_ptr, FPU_st0_ptr, control_word);
}



void
fadd_i()
{
	/* fadd st(i),st */
	reg_add(FPU_st0_ptr, &st(FPU_rm), &st(FPU_rm), control_word);
}


void
fmul_i()
{
	/* fmul st(i),st */
	reg_mul(&st(FPU_rm), FPU_st0_ptr, &st(FPU_rm), control_word);
}


void
fsubri()
{
	/* fsubr st(i),st */
	/* This is the sense of the 80486 manual reg_sub(&st(FPU_rm),
	 * FPU_st0_ptr, &st(FPU_rm), control_word); */
	reg_sub(FPU_st0_ptr, &st(FPU_rm), &st(FPU_rm), control_word);
}


void
fsub_i()
{
	/* fsub st(i),st */
	/* This is the sense of the 80486 manual reg_sub(FPU_st0_ptr,
	 * &st(FPU_rm), &st(FPU_rm), control_word); */
	reg_sub(&st(FPU_rm), FPU_st0_ptr, &st(FPU_rm), control_word);
}


void
fdivri()
{
	/* fdivr st(i),st */
	reg_div(FPU_st0_ptr, &st(FPU_rm), &st(FPU_rm), control_word);
}


void
fdiv_i()
{
	/* fdiv st(i),st */
	reg_div(&st(FPU_rm), FPU_st0_ptr, &st(FPU_rm), control_word);
}



void
faddp_()
{
	/* faddp st(i),st */
	reg_add(FPU_st0_ptr, &st(FPU_rm), &st(FPU_rm), control_word);
	pop();
}


void
fmulp_()
{
	/* fmulp st(i),st */
	reg_mul(&st(FPU_rm), FPU_st0_ptr, &st(FPU_rm), control_word);
	pop();
}



void
fsubrp()
{
	/* fsubrp st(i),st */
	/* This is the sense of the 80486 manual reg_sub(&st(FPU_rm),
	 * FPU_st0_ptr, &st(FPU_rm), control_word); */
	reg_sub(FPU_st0_ptr, &st(FPU_rm), &st(FPU_rm), control_word);
	pop();
}


void
fsubp_()
{
	/* fsubp st(i),st */
	/* This is the sense of the 80486 manual reg_sub(FPU_st0_ptr,
	 * &st(FPU_rm), &st(FPU_rm), control_word); */
	reg_sub(&st(FPU_rm), FPU_st0_ptr, &st(FPU_rm), control_word);
	pop();
}


void
fdivrp()
{
	/* fdivrp st(i),st */
	reg_div(FPU_st0_ptr, &st(FPU_rm), &st(FPU_rm), control_word);
	pop();
}


void
fdivp_()
{
	/* fdivp st(i),st */
	reg_div(&st(FPU_rm), FPU_st0_ptr, &st(FPU_rm), control_word);
	pop();
}
