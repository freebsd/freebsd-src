/*
 *  reg_add_sub.c
 *
 * Functions to add or subtract two registers and put the result in a third.
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
 *     $Id: reg_add_sub.c,v 1.6 1997/02/22 09:29:20 peter Exp $
 *
 */

/*---------------------------------------------------------------------------+
 | For each function, the destination may be any FPU_REG, including one of   |
 | the source FPU_REGs.                                                      |
 +---------------------------------------------------------------------------*/

#include <gnu/i386/fpemul/reg_constant.h>
#include <gnu/i386/fpemul/control_w.h>


void
reg_add(FPU_REG * a, FPU_REG * b, FPU_REG * dest, int control_w)
{
	int     diff;

	if (!(a->tag | b->tag)) {
		/* Both registers are valid */
		if (!(a->sign ^ b->sign)) {
			/* signs are the same */
			reg_u_add(a, b, dest, control_w);
			dest->sign = a->sign;
			return;
		}
		/* The signs are different, so do a subtraction */
		diff = a->exp - b->exp;
		if (!diff) {
			diff = a->sigh - b->sigh;	/* Works only if ms bits
							 * are identical */
			if (!diff) {
				diff = a->sigl > b->sigl;
				if (!diff)
					diff = -(a->sigl < b->sigl);
			}
		}
		if (diff > 0) {
			reg_u_sub(a, b, dest, control_w);
			dest->sign = a->sign;
		} else
			if (diff == 0) {
				reg_move(&CONST_Z, dest);
				/* sign depends upon rounding mode */
				dest->sign = ((control_w & CW_RC) != RC_DOWN)
				    ? SIGN_POS : SIGN_NEG;
			} else {
				reg_u_sub(b, a, dest, control_w);
				dest->sign = b->sign;
			}
		return;
	} else {
		if ((a->tag == TW_NaN) || (b->tag == TW_NaN)) {
			real_2op_NaN(a, b, dest);
			return;
		} else
			if (a->tag == TW_Zero) {
				if (b->tag == TW_Zero) {
					char    different_signs = a->sign ^ b->sign;
					/* Both are zero, result will be zero. */
					reg_move(a, dest);
					if (different_signs) {
						/* Signs are different. */
						/* Sign of answer depends upon
						 * rounding mode. */
						dest->sign = ((control_w & CW_RC) != RC_DOWN)
						    ? SIGN_POS : SIGN_NEG;
					}
				} else {
#ifdef DENORM_OPERAND
					if ((b->tag == TW_Valid) && (b->exp <= EXP_UNDER) &&
					    denormal_operand())
						return;
#endif				/* DENORM_OPERAND */
					reg_move(b, dest);
				}
				return;
			} else
				if (b->tag == TW_Zero) {
#ifdef DENORM_OPERAND
					if ((a->tag == TW_Valid) && (a->exp <= EXP_UNDER) &&
					    denormal_operand())
						return;
#endif				/* DENORM_OPERAND */
					reg_move(a, dest);
					return;
				} else
					if (a->tag == TW_Infinity) {
						if (b->tag != TW_Infinity) {
#ifdef DENORM_OPERAND
							if ((b->tag == TW_Valid) && (b->exp <= EXP_UNDER) &&
							    denormal_operand())
								return;
#endif				/* DENORM_OPERAND */
							reg_move(a, dest);
							return;
						}
						if (a->sign == b->sign) {
							/* They are both + or
							 * - infinity */
							reg_move(a, dest);
							return;
						}
						arith_invalid(dest);	/* Infinity-Infinity is
									 * undefined. */
						return;
					} else
						if (b->tag == TW_Infinity) {
#ifdef DENORM_OPERAND
							if ((a->tag == TW_Valid) && (a->exp <= EXP_UNDER) &&
							    denormal_operand())
								return;
#endif				/* DENORM_OPERAND */
							reg_move(b, dest);
							return;
						}
	}
#ifdef PARANOID
	EXCEPTION(EX_INTERNAL | 0x101);
#endif
}


/* Subtract b from a.  (a-b) -> dest */
void
reg_sub(FPU_REG * a, FPU_REG * b, FPU_REG * dest, int control_w)
{
	int     diff;

	if (!(a->tag | b->tag)) {
		/* Both registers are valid */
		diff = a->exp - b->exp;
		if (!diff) {
			diff = a->sigh - b->sigh;	/* Works only if ms bits
							 * are identical */
			if (!diff) {
				diff = a->sigl > b->sigl;
				if (!diff)
					diff = -(a->sigl < b->sigl);
			}
		}
		switch (a->sign * 2 + b->sign) {
		case 0:	/* P - P */
		case 3:	/* N - N */
			if (diff > 0) {
				reg_u_sub(a, b, dest, control_w);
				dest->sign = a->sign;
			} else
				if (diff == 0) {
#ifdef DENORM_OPERAND
					if ((b->tag == TW_Valid) && (b->exp <= EXP_UNDER) &&
					    denormal_operand())
						return;
#endif				/* DENORM_OPERAND */
					reg_move(&CONST_Z, dest);
					/* sign depends upon rounding mode */
					dest->sign = ((control_w & CW_RC) != RC_DOWN)
					    ? SIGN_POS : SIGN_NEG;
				} else {
					reg_u_sub(b, a, dest, control_w);
					dest->sign = a->sign ^ SIGN_POS ^ SIGN_NEG;
				}
			return;
		case 1:	/* P - N */
			reg_u_add(a, b, dest, control_w);
			dest->sign = SIGN_POS;
			return;
		case 2:	/* N - P */
			reg_u_add(a, b, dest, control_w);
			dest->sign = SIGN_NEG;
			return;
		}
	} else {
		if ((a->tag == TW_NaN) || (b->tag == TW_NaN)) {
			real_2op_NaN(a, b, dest);
			return;
		} else
			if (b->tag == TW_Zero) {
				if (a->tag == TW_Zero) {
					char    same_signs = !(a->sign ^ b->sign);
					/* Both are zero, result will be zero. */
					reg_move(a, dest);	/* Answer for different
								 * signs. */
					if (same_signs) {
						/* Sign depends upon rounding
						 * mode */
						dest->sign = ((control_w & CW_RC) != RC_DOWN)
						    ? SIGN_POS : SIGN_NEG;
					}
				} else {
#ifdef DENORM_OPERAND
					if ((a->tag == TW_Valid) && (a->exp <= EXP_UNDER) &&
					    denormal_operand())
						return;
#endif				/* DENORM_OPERAND */
					reg_move(a, dest);
				}
				return;
			} else
				if (a->tag == TW_Zero) {
#ifdef DENORM_OPERAND
					if ((b->tag == TW_Valid) && (b->exp <= EXP_UNDER) &&
					    denormal_operand())
						return;
#endif				/* DENORM_OPERAND */
					reg_move(b, dest);
					dest->sign ^= SIGN_POS ^ SIGN_NEG;
					return;
				} else
					if (a->tag == TW_Infinity) {
						if (b->tag != TW_Infinity) {
#ifdef DENORM_OPERAND
							if ((b->tag == TW_Valid) && (b->exp <= EXP_UNDER) &&
							    denormal_operand())
								return;
#endif				/* DENORM_OPERAND */
							reg_move(a, dest);
							return;
						}
						/* Both args are Infinity */
						if (a->sign == b->sign) {
							arith_invalid(dest);	/* Infinity-Infinity is
										 * undefined. */
							return;
						}
						reg_move(a, dest);
						return;
					} else
						if (b->tag == TW_Infinity) {
#ifdef DENORM_OPERAND
							if ((a->tag == TW_Valid) && (a->exp <= EXP_UNDER) &&
							    denormal_operand())
								return;
#endif				/* DENORM_OPERAND */
							reg_move(b, dest);
							dest->sign ^= SIGN_POS ^ SIGN_NEG;
							return;
						}
	}
#ifdef PARANOID
	EXCEPTION(EX_INTERNAL | 0x110);
#endif
}
