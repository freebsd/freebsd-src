/*
 *  reg_mul.c
 *
 * Multiply one FPU_REG by another, put the result in a destination FPU_REG.
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
 *     $Id: reg_mul.c,v 1.3 1994/06/10 07:44:53 rich Exp $
 *
 */

/*---------------------------------------------------------------------------+
 | The destination may be any FPU_REG, including one of the source FPU_REGs. |
 +---------------------------------------------------------------------------*/

#include "exception.h"
#include "reg_constant.h"
#include "fpu_emu.h"
#include "fpu_system.h"


/* This routine must be called with non-empty source registers */
void
reg_mul(FPU_REG * a, FPU_REG * b, FPU_REG * dest, unsigned int control_w)
{
	char    sign = (a->sign ^ b->sign);

	if (!(a->tag | b->tag)) {
		/* This should be the most common case */
		reg_u_mul(a, b, dest, control_w);
		dest->sign = sign;
		return;
	} else
		if ((a->tag <= TW_Zero) && (b->tag <= TW_Zero)) {
#ifdef DENORM_OPERAND
			if (((b->tag == TW_Valid) && (b->exp <= EXP_UNDER)) ||
			    ((a->tag == TW_Valid) && (a->exp <= EXP_UNDER))) {
				if (denormal_operand())
					return;
			}
#endif				/* DENORM_OPERAND */
			/* Must have either both arguments == zero, or one
			 * valid and the other zero. The result is therefore
			 * zero. */
			reg_move(&CONST_Z, dest);
#ifdef PECULIAR_486
			/* The 80486 book says that the answer is +0, but a
			 * real 80486 appears to behave this way... */
			dest->sign = sign;
#endif				/* PECULIAR_486 */
			return;
		}
#if 0				/* TW_Denormal is not used yet... perhaps
				 * never will be. */
		else
			if ((a->tag <= TW_Denormal) && (b->tag <= TW_Denormal)) {
				/* One or both arguments are de-normalized */
				/* Internal de-normalized numbers are not
				 * supported yet */
				EXCEPTION(EX_INTERNAL | 0x105);
				reg_move(&CONST_Z, dest);
			}
#endif
			else {
				/* Must have infinities, NaNs, etc */
				if ((a->tag == TW_NaN) || (b->tag == TW_NaN)) {
					real_2op_NaN(a, b, dest);
					return;
				} else
					if (a->tag == TW_Infinity) {
						if (b->tag == TW_Zero) {
							arith_invalid(dest);
							return;
						}
						/* Zero*Infinity is invalid */
						else {
#ifdef DENORM_OPERAND
							if ((b->tag == TW_Valid) && (b->exp <= EXP_UNDER) &&
							    denormal_operand())
								return;
#endif				/* DENORM_OPERAND */
							reg_move(a, dest);
							dest->sign = sign;
						}
						return;
					} else
						if (b->tag == TW_Infinity) {
							if (a->tag == TW_Zero) {
								arith_invalid(dest);
								return;
							}
							/* Zero*Infinity is
							 * invalid */
							else {
#ifdef DENORM_OPERAND
								if ((a->tag == TW_Valid) && (a->exp <= EXP_UNDER) &&
								    denormal_operand())
									return;
#endif				/* DENORM_OPERAND */
								reg_move(b, dest);
								dest->sign = sign;
							}
							return;
						}
#ifdef PARANOID
						else {
							EXCEPTION(EX_INTERNAL | 0x102);
						}
#endif				/* PARANOID */
			}
}
