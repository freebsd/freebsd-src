/*
 *  poly_2xm1.c
 *
 * Function to compute 2^x-1 by a polynomial approximation.
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



#define	HIPOWER	13
static unsigned short lterms[HIPOWER][4] =
{
	{0x79b5, 0xd1cf, 0x17f7, 0xb172},
	{0x1b56, 0x058b, 0x7bff, 0x3d7f},
	{0x8bb0, 0x8250, 0x846b, 0x0e35},
	{0xbc65, 0xf747, 0x556d, 0x0276},
	{0x17cb, 0x9e39, 0x61ff, 0x0057},
	{0xe018, 0x9776, 0x1848, 0x000a},
	{0x66f2, 0xff30, 0xffe5, 0x0000},
	{0x682f, 0xffb6, 0x162b, 0x0000},
	{0xb7ca, 0x2956, 0x01b5, 0x0000},
	{0xcd3e, 0x4817, 0x001e, 0x0000},
	{0xb7e2, 0xecbe, 0x0001, 0x0000},
	{0x0ed5, 0x1a27, 0x0000, 0x0000},
	{0x101d, 0x0222, 0x0000, 0x0000},
};


/*--- poly_2xm1() -----------------------------------------------------------+
 |                                                                           |
 +---------------------------------------------------------------------------*/
int
poly_2xm1(FPU_REG * arg, FPU_REG * result)
{
	short   exponent;
	long long Xll;
	FPU_REG accum;


	exponent = arg->exp - EXP_BIAS;

	if (arg->tag == TW_Zero) {
		/* Return 0.0 */
		reg_move(&CONST_Z, result);
		return 0;
	}
	if (exponent >= 0) {	/* Can't hack a number >= 1.0 */
		arith_invalid(result);	/* Number too large */
		return 1;
	}
	if (arg->sign != SIGN_POS) {	/* Can't hack a number < 0.0 */
		arith_invalid(result);	/* Number negative */
		return 1;
	}
	if (exponent < -64) {
		reg_move(&CONST_LN2, result);
		return 0;
	}
	*(unsigned *) &Xll = arg->sigl;
	*(((unsigned *) &Xll) + 1) = arg->sigh;
	if (exponent < -1) {
		/* shift the argument right by the required places */
		if (shrx(&Xll, -1 - exponent) >= (unsigned)0x80000000)
			Xll++;	/* round up */
	}
	*(short *) &(accum.sign) = 0;	/* will be a valid positive nr with
					 * expon = 0 */
	accum.exp = 0;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((unsigned *) &accum.sigl, (unsigned *) &Xll, lterms, HIPOWER - 1);

	/* Convert to 64 bit signed-compatible */
	accum.exp += EXP_BIAS - 1;

	reg_move(&accum, result);

	normalize(result);

	return 0;

}
