/*
 *  p_atan.c
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


#define	HIPOWERon	6	/* odd poly, negative terms */
static unsigned oddnegterms[HIPOWERon][2] =
{
	{0x00000000, 0x00000000},	/* for + 1.0 */
	{0x763b6f3d, 0x1adc4428},
	{0x20f0630b, 0x0502909d},
	{0x4e825578, 0x0198ce38},
	{0x22b7cb87, 0x008da6e3},
	{0x9b30ca03, 0x00239c79}
};
#define	HIPOWERop	6	/* odd poly, positive terms */
static unsigned oddplterms[HIPOWERop][2] =
{
	{0xa6f67cb8, 0x94d910bd},
	{0xa02ffab4, 0x0a43cb45},
	{0x04265e6b, 0x02bf5655},
	{0x0a728914, 0x00f280f7},
	{0x6d640e01, 0x004d6556},
	{0xf1dd2dbf, 0x000a530a}
};


static unsigned denomterm[2] =
{0xfc4bd208, 0xea2e6612};


static void poly_add_1(FPU_REG * src);

/*--- poly_atan() -----------------------------------------------------------+
 |                                                                           |
 +---------------------------------------------------------------------------*/
void
poly_atan(FPU_REG * arg)
{
	char    recursions = 0;
	short   exponent;
	FPU_REG odd_poly, even_poly, pos_poly, neg_poly;
	FPU_REG argSq;
	long long arg_signif, argSqSq;


#ifdef PARANOID
	if (arg->sign != 0) {	/* Can't hack a number < 0.0 */
		arith_invalid(arg);
		return;
	}			/* Need a positive number */
#endif				/* PARANOID */

	exponent = arg->exp - EXP_BIAS;

	if (arg->tag == TW_Zero) {
		/* Return 0.0 */
		reg_move(&CONST_Z, arg);
		return;
	}
	if (exponent >= -2) {
		/* argument is in the range  [0.25 .. 1.0] */
		if (exponent >= 0) {
#ifdef PARANOID
			if ((exponent == 0) &&
			    (arg->sigl == 0) && (arg->sigh == 0x80000000))
#endif				/* PARANOID */
			{
				reg_move(&CONST_PI4, arg);
				return;
			}
#ifdef PARANOID
			EXCEPTION(EX_INTERNAL | 0x104);	/* There must be a logic
							 * error */
#endif				/* PARANOID */
		}
		/* If the argument is greater than sqrt(2)-1 (=0.414213562...) */
		/* convert the argument by an identity for atan */
		if ((exponent >= -1) || (arg->sigh > 0xd413ccd0)) {
			FPU_REG numerator, denom;

			recursions++;

			arg_signif = *(long long *) &(arg->sigl);
			if (exponent < -1) {
				if (shrx(&arg_signif, -1 - exponent) >= (unsigned)0x80000000)
					arg_signif++;	/* round up */
			}
			*(long long *) &(numerator.sigl) = -arg_signif;
			numerator.exp = EXP_BIAS - 1;
			normalize(&numerator);	/* 1 - arg */

			arg_signif = *(long long *) &(arg->sigl);
			if (shrx(&arg_signif, -exponent) >= (unsigned)0x80000000)
				arg_signif++;	/* round up */
			*(long long *) &(denom.sigl) = arg_signif;
			denom.sigh |= 0x80000000;	/* 1 + arg */

			arg->exp = numerator.exp;
			reg_u_div(&numerator, &denom, arg, FULL_PRECISION);

			exponent = arg->exp - EXP_BIAS;
		}
	}
	*(long long *) &arg_signif = *(long long *) &(arg->sigl);

#ifdef PARANOID
	/* This must always be true */
	if (exponent >= -1) {
		EXCEPTION(EX_INTERNAL | 0x120);	/* There must be a logic error */
	}
#endif				/* PARANOID */

	/* shift the argument right by the required places */
	if (shrx(&arg_signif, -1 - exponent) >= (unsigned)0x80000000)
		arg_signif++;	/* round up */

	/* Now have arg_signif with binary point at the left .1xxxxxxxx */
	mul64(&arg_signif, &arg_signif, (long long *) (&argSq.sigl));
	mul64((long long *) (&argSq.sigl), (long long *) (&argSq.sigl), &argSqSq);

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(pos_poly.sign) = 0;
	pos_poly.exp = EXP_BIAS;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &pos_poly.sigl, (unsigned *) &argSqSq,
	    (unsigned short (*)[4]) oddplterms, HIPOWERop - 1);
	mul64((long long *) (&argSq.sigl), (long long *) (&pos_poly.sigl),
	    (long long *) (&pos_poly.sigl));

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(neg_poly.sign) = 0;
	neg_poly.exp = EXP_BIAS;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &neg_poly.sigl, (unsigned *) &argSqSq,
	    (unsigned short (*)[4]) oddnegterms, HIPOWERon - 1);

	/* Subtract the mantissas */
	*((long long *) (&pos_poly.sigl)) -= *((long long *) (&neg_poly.sigl));

	reg_move(&pos_poly, &odd_poly);
	poly_add_1(&odd_poly);

	/* The complete odd polynomial */
	reg_u_mul(&odd_poly, arg, &odd_poly, FULL_PRECISION);

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(even_poly.sign) = 0;

	mul64((long long *) (&argSq.sigl),
	    (long long *) (&denomterm), (long long *) (&even_poly.sigl));

	poly_add_1(&even_poly);

	reg_div(&odd_poly, &even_poly, arg, FULL_PRECISION);

	if (recursions)
		reg_sub(&CONST_PI4, arg, arg, FULL_PRECISION);
}


/* The argument to this function must be polynomial() compatible,
   i.e. have an exponent (not checked) of EXP_BIAS-1 but need not
   be normalized.
   This function adds 1.0 to the (assumed positive) argument. */
static void
poly_add_1(FPU_REG * src)
{
/* Rounding in a consistent direction produces better results
   for the use of this function in poly_atan. Simple truncation
   is used here instead of round-to-nearest. */

#ifdef OBSOLETE
	char    round = (src->sigl & 3) == 3;
#endif				/* OBSOLETE */

	shrx(&src->sigl, 1);

#ifdef OBSOLETE
	if (round)
		(*(long long *) &src->sigl)++;	/* Round to even */
#endif				/* OBSOLETE */

	src->sigh |= 0x80000000;

	src->exp = EXP_BIAS;

}
