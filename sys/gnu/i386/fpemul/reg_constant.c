/*
 *  reg_constant.c
 *
 * All of the constant FPU_REGs
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
 *     $id:$
 *
 */



#include "param.h"
#include "proc.h"
#include "systm.h"
#include "machine/cpu.h"
#include "machine/pcb.h"

#include "fpu_emu.h"
#include "fpu_system.h"
#include "status_w.h"
#include "reg_constant.h"


FPU_REG CONST_1 = {SIGN_POS, TW_Valid, EXP_BIAS,
0x00000000, 0x80000000};
static FPU_REG CONST_L2T = {SIGN_POS, TW_Valid, EXP_BIAS + 1,
0xcd1b8afe, 0xd49a784b};
static FPU_REG CONST_L2E = {SIGN_POS, TW_Valid, EXP_BIAS,
0x5c17f0bc, 0xb8aa3b29};
FPU_REG CONST_PI = {SIGN_POS, TW_Valid, EXP_BIAS + 1,
0x2168c235, 0xc90fdaa2};
FPU_REG CONST_PI2 = {SIGN_POS, TW_Valid, EXP_BIAS,
0x2168c235, 0xc90fdaa2};
FPU_REG CONST_PI4 = {SIGN_POS, TW_Valid, EXP_BIAS - 1,
0x2168c235, 0xc90fdaa2};
static FPU_REG CONST_LG2 = {SIGN_POS, TW_Valid, EXP_BIAS - 2,
0xfbcff799, 0x9a209a84};
FPU_REG CONST_LN2 = {SIGN_POS, TW_Valid, EXP_BIAS - 1,
0xd1cf79ac, 0xb17217f7};
/* Only the sign (and tag) is used in internal zeroes */
FPU_REG CONST_Z = {SIGN_POS, TW_Zero, 0, 0x0, 0x0};
/* Only the sign and significand (and tag) are used in internal NaNs */
/* The 80486 never generates one of these
FPU_REG CONST_SNAN = { SIGN_POS, TW_NaN, EXP_OVER, 0x00000001, 0x80000000 };
 */
/* This is the real indefinite QNaN */
FPU_REG CONST_QNaN = {SIGN_NEG, TW_NaN, EXP_OVER, 0x00000000, 0xC0000000};
/* Only the sign (and tag) is used in internal infinities */
FPU_REG CONST_INF = {SIGN_POS, TW_Infinity, EXP_OVER, 0x00000000, 0x80000000};



static void
fld_const(FPU_REG * c)
{
	FPU_REG *st_new_ptr;

	if (STACK_OVERFLOW) {
		stack_overflow();
		return;
	}
	push();
	reg_move(c, FPU_st0_ptr);
	status_word &= ~SW_C1;
}


static void
fld1(void)
{
	fld_const(&CONST_1);
}

static void
fldl2t(void)
{
	fld_const(&CONST_L2T);
}

static void
fldl2e(void)
{
	fld_const(&CONST_L2E);
}

static void
fldpi(void)
{
	fld_const(&CONST_PI);
}

static void
fldlg2(void)
{
	fld_const(&CONST_LG2);
}

static void
fldln2(void)
{
	fld_const(&CONST_LN2);
}

static void
fldz(void)
{
	fld_const(&CONST_Z);
}

static FUNC constants_table[] = {
	fld1, fldl2t, fldl2e, fldpi, fldlg2, fldln2, fldz, Un_impl
};

void
fconst(void)
{
	(constants_table[FPU_rm]) ();
}
