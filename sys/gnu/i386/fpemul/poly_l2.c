/*
 *  poly_l2.c
 *
 * Compute the base 2 log of a FPU_REG, using a polynomial approximation.
 *
 *
 * Copyright (C) 1992, 1993  W. Metzenthen, 22 Parker St, Ormond,
 *                           Vic 3163, Australia.
 *                           E-mail apm233m@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD operating system. Any other use is not permitted
 * under this copyright.
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
 */


#include "exception.h"
#include "reg_constant.h"
#include "fpu_emu.h"
#include "control_w.h"



#define	HIPOWER	9
static unsigned short lterms[HIPOWER][4] =
{
	/* Ideal computation with these coeffs gives about 64.6 bit rel
	 * accuracy. */
	{0xe177, 0xb82f, 0x7652, 0x7154},
	{0xee0f, 0xe80f, 0x2770, 0x7b1c},
	{0x0fc0, 0xbe87, 0xb143, 0x49dd},
	{0x78b9, 0xdadd, 0xec54, 0x34c2},
	{0x003a, 0x5de9, 0x628b, 0x2909},
	{0x5588, 0xed16, 0x4abf, 0x2193},
	{0xb461, 0x85f7, 0x347a, 0x1c6a},
	{0x0975, 0x87b3, 0xd5bf, 0x1876},
	{0xe85c, 0xcec9, 0x84e7, 0x187d}
};




/*--- poly_l2() -------------------------------------------------------------+
 |   Base 2 logarithm by a polynomial approximation.                         |
 +---------------------------------------------------------------------------*/
void
poly_l2(FPU_REG * arg, FPU_REG * result)
{
	short   exponent;
	char    zero;		/* flag for an Xx == 0 */
	unsigned short bits, shift;
	long long Xsq;
	FPU_REG accum, denom, num, Xx;


	exponent = arg->exp - EXP_BIAS;

	accum.tag = TW_Valid;	/* set the tags to Valid */

	if (arg->sigh > (unsigned) 0xb504f334) {
		/* This is good enough for the computation of the polynomial
		 * sum, but actually results in a loss of precision for the
		 * computation of Xx. This will matter only if exponent
		 * becomes zero. */
		exponent++;
		accum.sign = 1;	/* sign to negative */
		num.exp = EXP_BIAS;	/* needed to prevent errors in div
					 * routine */
		reg_u_div(&CONST_1, arg, &num, FULL_PRECISION);
	} else {
		accum.sign = 0;	/* set the sign to positive */
		num.sigl = arg->sigl;	/* copy the mantissa */
		num.sigh = arg->sigh;
	}


	/* shift num left, lose the ms bit */
	num.sigh <<= 1;
	if (num.sigl & 0x80000000)
		num.sigh |= 1;
	num.sigl <<= 1;

	denom.sigl = num.sigl;
	denom.sigh = num.sigh;
	poly_div4((long long *) &(denom.sigl));
	denom.sigh += 0x80000000;	/* set the msb */
	Xx.exp = EXP_BIAS;	/* needed to prevent errors in div routine */
	reg_u_div(&num, &denom, &Xx, FULL_PRECISION);

	zero = !(Xx.sigh | Xx.sigl);

	mul64((long long *) &Xx.sigl, (long long *) &Xx.sigl, &Xsq);
	poly_div16(&Xsq);

	accum.exp = -1;		/* exponent of accum */

	/* Do the basic fixed point polynomial evaluation */
	polynomial((unsigned *) &accum.sigl, (unsigned *) &Xsq, lterms, HIPOWER - 1);

	if (!exponent) {
		/* If the exponent is zero, then we would lose precision by
		 * sticking to fixed point computation here */
		/* We need to re-compute Xx because of loss of precision. */
		FPU_REG lXx;
		char    sign;

		sign = accum.sign;
		accum.sign = 0;

		/* make accum compatible and normalize */
		accum.exp = EXP_BIAS + accum.exp;
		normalize(&accum);

		if (zero) {
			reg_move(&CONST_Z, result);
		} else {
			/* we need to re-compute lXx to better accuracy */
			num.tag = TW_Valid;	/* set the tags to Vaild */
			num.sign = 0;	/* set the sign to positive */
			num.exp = EXP_BIAS - 1;
			if (sign) {
				/* The argument is of the form 1-x */
				/* Use  1-1/(1-x) = x/(1-x) */
				*((long long *) &num.sigl) = -*((long long *) &(arg->sigl));
				normalize(&num);
				reg_div(&num, arg, &num, FULL_PRECISION);
			} else {
				normalize(&num);
			}

			denom.tag = TW_Valid;	/* set the tags to Valid */
			denom.sign = SIGN_POS;	/* set the sign to positive */
			denom.exp = EXP_BIAS;

			reg_div(&num, &denom, &lXx, FULL_PRECISION);

			reg_u_mul(&lXx, &accum, &accum, FULL_PRECISION);

			reg_u_add(&lXx, &accum, result, FULL_PRECISION);

			normalize(result);
		}

		result->sign = sign;
		return;
	}
	mul64((long long *) &accum.sigl,
	    (long long *) &Xx.sigl, (long long *) &accum.sigl);

	*((long long *) (&accum.sigl)) += *((long long *) (&Xx.sigl));

	if (Xx.sigh > accum.sigh) {
		/* There was an overflow */

		poly_div2((long long *) &accum.sigl);
		accum.sigh |= 0x80000000;
		accum.exp++;
	}
	/* When we add the exponent to the accum result later, we will require
	 * that their signs are the same. Here we ensure that this is so. */
	if (exponent && ((exponent < 0) ^ (accum.sign))) {
		/* signs are different */

		accum.sign = !accum.sign;

		/* An exceptional case is when accum is zero */
		if (accum.sigl | accum.sigh) {
			/* find 1-accum */
			/* Shift to get exponent == 0 */
			if (accum.exp < 0) {
				poly_div2((long long *) &accum.sigl);
				accum.exp++;
			}
			/* Just negate, but throw away the sign */
			*((long long *) &(accum.sigl)) = -*((long long *) &(accum.sigl));
			if (exponent < 0)
				exponent++;
			else
				exponent--;
		}
	}
	shift = exponent >= 0 ? exponent : -exponent;
	bits = 0;
	if (shift) {
		if (accum.exp) {
			accum.exp++;
			poly_div2((long long *) &accum.sigl);
		}
		while (shift) {
			poly_div2((long long *) &accum.sigl);
			if (shift & 1)
				accum.sigh |= 0x80000000;
			shift >>= 1;
			bits++;
		}
	}
	/* Convert to 64 bit signed-compatible */
	accum.exp += bits + EXP_BIAS - 1;

	reg_move(&accum, result);
	normalize(result);

	return;
}


/*--- poly_l2p1() -----------------------------------------------------------+
 |   Base 2 logarithm by a polynomial approximation.                         |
 |   log2(x+1)                                                               |
 +---------------------------------------------------------------------------*/
int
poly_l2p1(FPU_REG * arg, FPU_REG * result)
{
	char    sign = 0;
	long long Xsq;
	FPU_REG arg_pl1, denom, accum, local_arg, poly_arg;


	sign = arg->sign;

	reg_add(arg, &CONST_1, &arg_pl1, FULL_PRECISION);

	if ((arg_pl1.sign) | (arg_pl1.tag)) {	/* We need a valid positive
						 * number! */
		return 1;
	}
	reg_add(&CONST_1, &arg_pl1, &denom, FULL_PRECISION);
	reg_div(arg, &denom, &local_arg, FULL_PRECISION);
	local_arg.sign = 0;	/* Make the sign positive */

	/* Now we need to check that  |local_arg| is less than 3-2*sqrt(2) =
	 * 0.17157.. = .0xafb0ccc0 * 2^-2 */

	if (local_arg.exp >= EXP_BIAS - 3) {
		if ((local_arg.exp > EXP_BIAS - 3) ||
		    (local_arg.sigh > (unsigned) 0xafb0ccc0)) {
			/* The argument is large */
			poly_l2(&arg_pl1, result);
			return 0;
		}
	}
	/* Make a copy of local_arg */
	reg_move(&local_arg, &poly_arg);

	/* Get poly_arg bits aligned as required */
	shrx((unsigned *) &(poly_arg.sigl), -(poly_arg.exp - EXP_BIAS + 3));

	mul64((long long *) &(poly_arg.sigl), (long long *) &(poly_arg.sigl), &Xsq);
	poly_div16(&Xsq);

	/* Do the basic fixed point polynomial evaluation */
	polynomial(&(accum.sigl), (unsigned *) &Xsq, lterms, HIPOWER - 1);

	accum.tag = TW_Valid;	/* set the tags to Valid */
	accum.sign = SIGN_POS;	/* and make accum positive */

	/* make accum compatible and normalize */
	accum.exp = EXP_BIAS - 1;
	normalize(&accum);

	reg_u_mul(&local_arg, &accum, &accum, FULL_PRECISION);

	reg_u_add(&local_arg, &accum, result, FULL_PRECISION);

	/* Multiply the result by 2 */
	result->exp++;

	result->sign = sign;

	return 0;
}
