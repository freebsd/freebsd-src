/*
 *  poly_sin.c
 *
 *  Computation of an approximation of the sin function by a polynomial
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
 *     $FreeBSD$
 *
 */


#include <gnu/i386/fpemul/exception.h>
#include <gnu/i386/fpemul/reg_constant.h>
#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/control_w.h>


#define	HIPOWER	5
static unsigned short lterms[HIPOWER][4] =
{
	{0x846a, 0x42d1, 0xb544, 0x921f},
	{0xe110, 0x75aa, 0xbc67, 0x1466},
	{0x503d, 0xa43f, 0x83c1, 0x000a},
	{0x8f9d, 0x7a19, 0x00f4, 0x0000},
	{0xda03, 0x06aa, 0x0000, 0x0000},
};

static unsigned short negterms[HIPOWER][4] =
{
	{0x95ed, 0x2df2, 0xe731, 0xa55d},
	{0xd159, 0xe62b, 0xd2cc, 0x0132},
	{0x6342, 0xe9fb, 0x3c60, 0x0000},
	{0x6256, 0xdf5a, 0x0002, 0x0000},
	{0xf279, 0x000b, 0x0000, 0x0000},
};


/*--- poly_sine() -----------------------------------------------------------+
 |                                                                           |
 +---------------------------------------------------------------------------*/
void
poly_sine(FPU_REG * arg, FPU_REG * result)
{
	short   exponent;
	FPU_REG Xx, Xx2, Xx4, accum, negaccum;


	exponent = arg->exp - EXP_BIAS;

	if (arg->tag == TW_Zero) {
		/* Return 0.0 */
		reg_move(&CONST_Z, result);
		return;
	}
#ifdef PARANOID
	if (arg->sign != 0) {	/* Can't hack a number < 0.0 */
		EXCEPTION(EX_Invalid);
		reg_move(&CONST_QNaN, result);
		return;
	}
	if (exponent >= 0) {	/* Can't hack a number > 1.0 */
		if ((exponent == 0) && (arg->sigl == 0) && (arg->sigh == 0x80000000)) {
			reg_move(&CONST_1, result);
			return;
		}
		EXCEPTION(EX_Invalid);
		reg_move(&CONST_QNaN, result);
		return;
	}
#endif				/* PARANOID */

	Xx.sigl = arg->sigl;
	Xx.sigh = arg->sigh;
	if (exponent < -1) {
		/* shift the argument right by the required places */
		if (shrx(&(Xx.sigl), -1 - exponent) >= (unsigned)0x80000000)
			(*((long long *) (&(Xx.sigl))))++;	/* round up */
	}
	mul64((long long *) &(Xx.sigl), (long long *) &(Xx.sigl),
	    (long long *) &(Xx2.sigl));
	mul64((long long *) &(Xx2.sigl), (long long *) &(Xx2.sigl),
	    (long long *) &(Xx4.sigl));

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(accum.sign) = 0;
	accum.exp = 0;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &(accum.sigl), (u_int *)&(Xx4.sigl), lterms, HIPOWER - 1);

	/* will be a valid positive nr with expon = 0 */
	*(short *) &(negaccum.sign) = 0;
	negaccum.exp = 0;

	/* Do the basic fixed point polynomial evaluation */
	polynomial((u_int *) &(negaccum.sigl), (u_int *)&(Xx4.sigl), negterms, HIPOWER - 1);
	mul64((long long *) &(Xx2.sigl), (long long *) &(negaccum.sigl),
	    (long long *) &(negaccum.sigl));

	/* Subtract the mantissas */
	*((long long *) (&(accum.sigl))) -= *((long long *) (&(negaccum.sigl)));

	/* Convert to 64 bit signed-compatible */
	accum.exp = EXP_BIAS - 1 + accum.exp;

	*(short *) &(result->sign) = *(short *) &(accum.sign);
	result->exp = accum.exp;
	result->sigl = accum.sigl;
	result->sigh = accum.sigh;

	normalize(result);

	reg_mul(result, arg, result, FULL_PRECISION);
	reg_u_add(result, arg, result, FULL_PRECISION);

	/* A small overflow may be possible... but an illegal result. */
	if (result->exp >= EXP_BIAS) {
		if ((result->exp > EXP_BIAS)	/* Larger or equal 2.0 */
		    ||(result->sigl > 1)	/* Larger than 1.0+msb */
		    ||(result->sigh != 0x80000000)	/* Much > 1.0 */
		    ) {
#ifdef DEBUGGING
			RE_ENTRANT_CHECK_OFF
			    printk("\nEXP=%d, MS=%08x, LS=%08x\n", result->exp,
			    result->sigh, result->sigl);
			RE_ENTRANT_CHECK_ON
#endif				/* DEBUGGING */
			    EXCEPTION(EX_INTERNAL | 0x103);
		}
#ifdef DEBUGGING
		RE_ENTRANT_CHECK_OFF
		    printk("\n***CORRECTING ILLEGAL RESULT*** in poly_sin() computation\n");
		printk("EXP=%d, MS=%08x, LS=%08x\n", result->exp,
		    result->sigh, result->sigl);
		RE_ENTRANT_CHECK_ON
#endif				/* DEBUGGING */

		    result->sigl = 0;	/* Truncate the result to 1.00 */
	}
}
