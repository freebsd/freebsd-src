/*
 *  poly_tan.c
 *
 * Compute the tan of a FPU_REG, using a polynomial approximation.
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
 * $FreeBSD$
 *
 */

#include <gnu/i386/fpemul/reg_constant.h>
#include <gnu/i386/fpemul/control_w.h>


#define	HIPOWERop	3	/* odd poly, positive terms */
static unsigned short oddplterms[HIPOWERop][4] =
{
	{0x846a, 0x42d1, 0xb544, 0x921f},
	{0x6fb2, 0x0215, 0x95c0, 0x099c},
	{0xfce6, 0x0cc8, 0x1c9a, 0x0000}
};
#define	HIPOWERon	2	/* odd poly, negative terms */
static unsigned short oddnegterms[HIPOWERon][4] =
{
	{0x6906, 0xe205, 0x25c8, 0x8838},
	{0x1dd7, 0x3fe3, 0x944e, 0x002c}
};
#define	HIPOWERep	2	/* even poly, positive terms */
static unsigned short evenplterms[HIPOWERep][4] =
{
	{0xdb8f, 0x3761, 0x1432, 0x2acf},
	{0x16eb, 0x13c1, 0x3099, 0x0003}
};
#define	HIPOWERen	2	/* even poly, negative terms */
static unsigned short evennegterms[HIPOWERen][4] =
{
	{0x3a7c, 0xe4c5, 0x7f87, 0x2945},
	{0x572b, 0x664c, 0xc543, 0x018c}
};


/*--- poly_tan() ------------------------------------------------------------+
 |                                                                           |
 +---------------------------------------------------------------------------*/
void
poly_tan(FPU_REG * arg, FPU_REG * y_reg)
{
	char    invert = 0;
	short   exponent;
	FPU_REG odd_poly, even_poly, pos_poly, neg_poly;
	FPU_REG argSq;
	long long arg_signif, argSqSq;


	exponent = arg->exp - EXP_BIAS;

	if (arg->tag == TW_Zero) {
		/* Return 0.0 */
		reg_move(&CONST_Z, y_reg);
		return;
	}
	if (exponent >= -1) {
		/* argument is in the range  [0.5 .. 1.0] */
		if (exponent >= 0) {
#ifdef PARANOID
			if ((exponent == 0) &&
			    (arg->sigl == 0) && (arg->sigh == 0x80000000))
#endif				/* PARANOID */
			{
				arith_overflow(y_reg);
				return;
			}
#ifdef PARANOID
			EXCEPTION(EX_INTERNAL | 0x104);	/* There must be a logic
							 * error */
			return;
#endif				/* PARANOID */
		}
		/* The argument is in the range  [0.5 .. 1.0) */
		/* Convert the argument to a number in the range  (0.0 .. 0.5] */
		*((long long *) (&arg->sigl)) = -*((long long *) (&arg->sigl));
		normalize(arg);	/* Needed later */
		exponent = arg->exp - EXP_BIAS;
		invert = 1;
	}
#ifdef PARANOID
	if (arg->sign != 0) {	/* Can't hack a number < 0.0 */
		arith_invalid(y_reg);
		return;
	}			/* Need a positive number */
#endif				/* PARANOID */

	*(long long *) &arg_signif = *(long long *) &(arg->sigl);
	if (exponent < -1) {
		/* shift the argument right by the required places */
		if (shrx(&arg_signif, -1 - exponent) >= (unsigned)0x80000000)
			arg_signif++;	/* round up */
	}
	mul64(&arg_signif, &arg_signif, (long long *) (&argSq.sigl));
	mul64((long long *) (&argSq.sigl), (long long *) (&argSq.sigl), &argSqSq);

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(pos_poly.sign) = 0;
	pos_poly.exp = EXP_BIAS;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &pos_poly.sigl, (unsigned *) &argSqSq, oddplterms, HIPOWERop - 1);

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(neg_poly.sign) = 0;
	neg_poly.exp = EXP_BIAS;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &neg_poly.sigl, (unsigned *) &argSqSq, oddnegterms, HIPOWERon - 1);
	mul64((long long *) (&argSq.sigl), (long long *) (&neg_poly.sigl),
	    (long long *) (&neg_poly.sigl));

	/* Subtract the mantissas */
	*((long long *) (&pos_poly.sigl)) -= *((long long *) (&neg_poly.sigl));

	/* Convert to 64 bit signed-compatible */
	pos_poly.exp -= 1;

	reg_move(&pos_poly, &odd_poly);
	normalize(&odd_poly);

	reg_mul(&odd_poly, arg, &odd_poly, FULL_PRECISION);
	reg_u_add(&odd_poly, arg, &odd_poly, FULL_PRECISION);	/* This is just the odd
								 * polynomial */


	/* will be a valid positive nr with expon = 0 */
	*(short *) &(pos_poly.sign) = 0;
	pos_poly.exp = EXP_BIAS;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &pos_poly.sigl, (unsigned *) &argSqSq, evenplterms, HIPOWERep - 1);
	mul64((long long *) (&argSq.sigl),
	    (long long *) (&pos_poly.sigl), (long long *) (&pos_poly.sigl));

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(neg_poly.sign) = 0;
	neg_poly.exp = EXP_BIAS;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &neg_poly.sigl, (unsigned *) &argSqSq, evennegterms, HIPOWERen - 1);

	/* Subtract the mantissas */
	*((long long *) (&neg_poly.sigl)) -= *((long long *) (&pos_poly.sigl));
	/* and multiply by argSq */

	/* Convert argSq to a valid reg number */
	*(short *) &(argSq.sign) = 0;
	argSq.exp = EXP_BIAS - 1;
	normalize(&argSq);

	/* Convert to 64 bit signed-compatible */
	neg_poly.exp -= 1;

	reg_move(&neg_poly, &even_poly);
	normalize(&even_poly);

	reg_mul(&even_poly, &argSq, &even_poly, FULL_PRECISION);
	reg_add(&even_poly, &argSq, &even_poly, FULL_PRECISION);
	reg_sub(&CONST_1, &even_poly, &even_poly, FULL_PRECISION);	/* This is just the even
									 * polynomial */

	/* Now ready to copy the results */
	if (invert) {
		reg_div(&even_poly, &odd_poly, y_reg, FULL_PRECISION);
	} else {
		reg_div(&odd_poly, &even_poly, y_reg, FULL_PRECISION);
	}

}
