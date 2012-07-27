/*-
 * Copyright (c) 2009-2012 Steven G. Kargl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Optimized by Bruce D. Evans.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Compute the exponential of x for Intel 80-bit format.  This is based on:
 *
 *   PTP Tang, "Table-driven implementation of the exponential function
 *   in IEEE floating-point arithmetic," ACM Trans. Math. Soft., 15,
 *   144-157 (1989).
 *
 * where the 32 table entries have been expanded to INTERVALS (see below).
 */

#include <float.h>

#ifdef __i386__
#include <ieeefp.h>
#endif

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

#define	BIAS	(LDBL_MAX_EXP - 1)

static const long double
huge = 0x1p10000L,
twom10000 = 0x1p-10000L;
/* XXX Prevent gcc from erroneously constant folding this: */
static volatile const long double tiny = 0x1p-10000L;

static const union IEEEl2bits
/* log(2**16384 - 0.5) rounded towards zero: */
o_threshold = LD80C(0xb17217f7d1cf79ab, 13, 0,  11356.5234062941439488L),
/* log(2**(-16381-64-1)) rounded towards zero: */
u_threshold = LD80C(0xb21dfe7f09e2baa9, 13, 1, -11399.4985314888605581L);

static const double __aligned(64)
/*
 * ln2/INTERVALS = L1+L2 (hi+lo decomposition for multiplication).  L1 must
 * have at least 22 (= log2(|LDBL_MIN_EXP-extras|) + log2(INTERVALS)) lowest
 * bits zero so that multiplication of it by n is exact.
 */
L1 =  5.4152123484527692e-3,		/*  0x162e42ff000000.0p-60 */
L2 = -3.2819649005320973e-13,		/* -0x1718432a1b0e26.0p-94 */
INV_L = 1.8466496523378731e+2,		/*  0x171547652b82fe.0p-45 */
/*
 * Domain [-0.002708, 0.002708], range ~[-5.7136e-24, 5.7110e-24]:
 * |exp(x) - p(x)| < 2**-77.2
 * (0.002708 is ln2/(2*INTERVALS) rounded up a little).
 */
P2 =  0.5,
P3 =  1.6666666666666119e-1,		/*  0x15555555555490.0p-55 */
P4 =  4.1666666666665887e-2,		/*  0x155555555554e5.0p-57 */
P5 =  8.3333354987869413e-3,		/*  0x1111115b789919.0p-59 */
P6 =  1.3888891738560272e-3;		/*  0x16c16c651633ae.0p-62 */

/*
 * 2^(i/INTERVALS) for i in [0,INTERVALS] is represented by two values where
 * the first 47 (?!) bits of the significand is stored in hi and the next 53
 * bits are in lo.
 */
#define	INTERVALS		128

static const struct {
	double	hi;
	double	lo;
} s[INTERVALS] __aligned(16) = {
	0x1p+0, 0x0p+0,
	0x1.0163da9fb330p+0, 0x1.ab6c25335719bp-47,
	0x1.02c9a3e77804p+0, 0x1.07737be56527cp-47,
	0x1.04315e86e7f8p+0, 0x1.2f5ce3e688369p-50,
	0x1.059b0d315854p+0, 0x1.a1d73e2a475b4p-47,
	0x1.0706b29ddf6cp+0, 0x1.dc6dc403a9d88p-48,
	0x1.0874518759bcp+0, 0x1.01186be4bb285p-49,
	0x1.09e3ecac6f38p+0, 0x1.a290f03062c27p-51,
	0x1.0b5586cf9890p+0, 0x1.ec5317256e308p-49,
	0x1.0cc922b7247cp+0, 0x1.ba03db82dc49fp-47,
	0x1.0e3ec32d3d18p+0, 0x1.10103a1727c58p-47,
	0x1.0fb66affed30p+0, 0x1.af232091dd8a1p-48,
	0x1.11301d0125b4p+0, 0x1.0a4ebbf1aed93p-48,
	0x1.12abdc06c31cp+0, 0x1.7f72575a649adp-49,
	0x1.1429aaea92dcp+0, 0x1.fb34101943b26p-48,
	0x1.15a98c8a58e4p+0, 0x1.12480d573dd56p-48,
	0x1.172b83c7d514p+0, 0x1.d6e6fbe462876p-47,
	0x1.18af9388c8dcp+0, 0x1.4dddfb85cd1e1p-47,
	0x1.1a35beb6fcb4p+0, 0x1.a9e5b4c7b4969p-47,
	0x1.1bbe084045ccp+0, 0x1.39ab1e72b4428p-48,
	0x1.1d4873168b98p+0, 0x1.53c02dc0144c8p-47,
	0x1.1ed5022fcd90p+0, 0x1.cb8819ff61122p-48,
	0x1.2063b88628ccp+0, 0x1.63b8eeb029509p-48,
	0x1.21f49917ddc8p+0, 0x1.62552fd29294cp-48,
	0x1.2387a6e75620p+0, 0x1.c3360fd6d8e0bp-47,
	0x1.251ce4fb2a60p+0, 0x1.f9ac155bef4f5p-47,
	0x1.26b4565e27ccp+0, 0x1.d257a673281d4p-48,
	0x1.284dfe1f5638p+0, 0x1.2d9e2b9e07941p-53,
	0x1.29e9df51fdecp+0, 0x1.09612e8afad12p-47,
	0x1.2b87fd0dad98p+0, 0x1.ffbbd48ca71f9p-49,
	0x1.2d285a6e4030p+0, 0x1.680123aa6da0fp-49,
	0x1.2ecafa93e2f4p+0, 0x1.611ca0f45d524p-48,
	0x1.306fe0a31b70p+0, 0x1.52de8d5a46306p-48,
	0x1.32170fc4cd80p+0, 0x1.89a9ce78e1804p-47,
	0x1.33c08b26416cp+0, 0x1.fa64e43086cb3p-47,
	0x1.356c55f929fcp+0, 0x1.864a311a3b1bap-47,
	0x1.371a7373aa9cp+0, 0x1.54e28aa05e8a9p-49,
	0x1.38cae6d05d84p+0, 0x1.2c2d4e586cdf7p-47,
	0x1.3a7db34e59fcp+0, 0x1.b750de494cf05p-47,
	0x1.3c32dc313a8cp+0, 0x1.242000f9145acp-47,
	0x1.3dea64c12340p+0, 0x1.11ada0911f09fp-47,
	0x1.3fa4504ac800p+0, 0x1.ba0bf701aa418p-48,
	0x1.4160a21f72e0p+0, 0x1.4fc2192dc79eep-47,
	0x1.431f5d950a88p+0, 0x1.6dc704439410dp-48,
	0x1.44e086061890p+0, 0x1.68189b7a04ef8p-47,
	0x1.46a41ed1d004p+0, 0x1.772512f45922ap-48,
	0x1.486a2b5c13ccp+0, 0x1.013c1a3b69063p-48,
	0x1.4a32af0d7d3cp+0, 0x1.e672d8bcf46f9p-48,
	0x1.4bfdad5362a0p+0, 0x1.38ea1cbd7f621p-47,
	0x1.4dcb299fddd0p+0, 0x1.ac766dde353c2p-49,
	0x1.4f9b2769d2c8p+0, 0x1.35699ec5b4d50p-47,
	0x1.516daa2cf664p+0, 0x1.c112f52c84d82p-52,
	0x1.5342b569d4f8p+0, 0x1.df0a83c49d86ap-52,
	0x1.551a4ca5d920p+0, 0x1.d8a5d8c40486ap-49,
	0x1.56f4736b527cp+0, 0x1.a66ecb004764fp-48,
	0x1.58d12d497c7cp+0, 0x1.e9295e15b9a1ep-47,
	0x1.5ab07dd48540p+0, 0x1.4ac64980a8c8fp-47,
	0x1.5c9268a59468p+0, 0x1.b80e258dc0b4cp-47,
	0x1.5e76f15ad214p+0, 0x1.0dd37c9840733p-49,
	0x1.605e1b976dc0p+0, 0x1.160edeb25490ep-49,
	0x1.6247eb03a558p+0, 0x1.2c7c3e81bf4b7p-50,
	0x1.6434634ccc30p+0, 0x1.fc76f8714c4eep-48,
	0x1.662388255220p+0, 0x1.24893ecf14dc8p-47,
	0x1.68155d44ca94p+0, 0x1.9840e2b913dd0p-47,
	0x1.6a09e667f3bcp+0, 0x1.921165f626cddp-49,
	0x1.6c012750bda8p+0, 0x1.f76bb54cc007ap-47,
	0x1.6dfb23c651a0p+0, 0x1.779107165f0dep-47,
	0x1.6ff7df951948p+0, 0x1.e7c3f0da79f11p-51,
	0x1.71f75e8ec5f4p+0, 0x1.9ee91b8797785p-47,
	0x1.73f9a48a5814p+0, 0x1.9deae4d273456p-47,
	0x1.75feb564267cp+0, 0x1.17edd35467491p-49,
	0x1.780694fde5d0p+0, 0x1.fb0cd7014042cp-47,
	0x1.7a11473eb018p+0, 0x1.b5f54408fdb37p-50,
	0x1.7c1ed0130c10p+0, 0x1.93e2499a22c9cp-47,
	0x1.7e2f336cf4e4p+0, 0x1.1082e815d0abdp-47,
	0x1.80427543e1a0p+0, 0x1.1b60de67649a3p-48,
	0x1.82589994cce0p+0, 0x1.28acf88afab35p-48,
	0x1.8471a4623c78p+0, 0x1.667297b5cbe32p-47,
	0x1.868d99b4492cp+0, 0x1.640720ec85613p-47,
	0x1.88ac7d98a668p+0, 0x1.966530bcdf2d5p-48,
	0x1.8ace5422aa0cp+0, 0x1.b5ba7c55a192dp-48,
	0x1.8cf3216b5448p+0, 0x1.7de55439a2c39p-49,
	0x1.8f1ae9915770p+0, 0x1.b15cc13a2e397p-47,
	0x1.9145b0b91ffcp+0, 0x1.622986d1a7daep-50,
	0x1.93737b0cdc5cp+0, 0x1.27a280e1f92a0p-47,
	0x1.95a44cbc8520p+0, 0x1.dd36906d2b420p-49,
	0x1.97d829fde4e4p+0, 0x1.f173d241f23d1p-49,
	0x1.9a0f170ca078p+0, 0x1.cdd1884dc6234p-47,
	0x1.9c49182a3f08p+0, 0x1.01c7c46b071f3p-48,
	0x1.9e86319e3230p+0, 0x1.18c12653c7326p-47,
	0x1.a0c667b5de54p+0, 0x1.2594d6d45c656p-47,
	0x1.a309bec4a2d0p+0, 0x1.9ac60b8fbb86dp-47,
	0x1.a5503b23e254p+0, 0x1.c8b424491caf8p-48,
	0x1.a799e1330b34p+0, 0x1.86f2dfb2b158fp-48,
	0x1.a9e6b5579fd8p+0, 0x1.fa1f5921deffap-47,
	0x1.ac36bbfd3f34p+0, 0x1.ce06dcb351893p-47,
	0x1.ae89f995ad38p+0, 0x1.6af439a68bb99p-47,
	0x1.b0e07298db64p+0, 0x1.2c8421566fe38p-47,
	0x1.b33a2b84f15cp+0, 0x1.d7b5fe873decap-47,
	0x1.b59728de5590p+0, 0x1.cc71c40888b24p-47,
	0x1.b7f76f2fb5e4p+0, 0x1.baa9ec206ad4fp-50,
	0x1.ba5b030a1064p+0, 0x1.30819678d5eb7p-49,
	0x1.bcc1e904bc1cp+0, 0x1.2247ba0f45b3dp-48,
	0x1.bf2c25bd71e0p+0, 0x1.10811ae04a31cp-49,
	0x1.c199bdd85528p+0, 0x1.c2220cb12a092p-48,
	0x1.c40ab5fffd04p+0, 0x1.d368a6fc1078cp-47,
	0x1.c67f12e57d14p+0, 0x1.694426ffa41e5p-49,
	0x1.c8f6d9406e78p+0, 0x1.a88d65e24402ep-47,
	0x1.cb720dcef904p+0, 0x1.48a81e5e8f4a5p-47,
	0x1.cdf0b555dc3cp+0, 0x1.ce227c4ac7d63p-47,
	0x1.d072d4a07894p+0, 0x1.dc68791790d0bp-47,
	0x1.d2f87080d89cp+0, 0x1.8c56f091cc4f5p-47,
	0x1.d5818dcfba48p+0, 0x1.c976816bad9b8p-50,
	0x1.d80e316c9838p+0, 0x1.7bb84f9d04880p-48,
	0x1.da9e603db328p+0, 0x1.5c2300696db53p-50,
	0x1.dd321f301b44p+0, 0x1.025b4aef1e032p-47,
	0x1.dfc97337b9b4p+0, 0x1.eb968cac39ed3p-48,
	0x1.e264614f5a10p+0, 0x1.45093b0fd0bd7p-47,
	0x1.e502ee78b3fcp+0, 0x1.b139e8980a9cdp-47,
	0x1.e7a51fbc74c8p+0, 0x1.a5aa4594191bcp-51,
	0x1.ea4afa2a490cp+0, 0x1.9858f73a18f5ep-48,
	0x1.ecf482d8e67cp+0, 0x1.846d81897dca5p-47,
	0x1.efa1bee615a0p+0, 0x1.3bb8fe90d496dp-47,
	0x1.f252b376bba8p+0, 0x1.74e8696fc3639p-48,
	0x1.f50765b6e454p+0, 0x1.9d3e12dd8a18bp-54,
	0x1.f7bfdad9cbe0p+0, 0x1.38913b4bfe72cp-48,
	0x1.fa7c1819e90cp+0, 0x1.82e90a7e74b26p-48,
	0x1.fd3c22b8f71cp+0, 0x1.884badd25995ep-47
};

long double
expl(long double x)
{
	union IEEEl2bits u, v;
	long double fn, r, r1, r2, q, t, t23, t45, twopk, twopkp10000, z;
	int k, n, n2;
	uint16_t hx, ix;

	/* Filter out exceptional cases. */
	u.e = x;
	hx = u.xbits.expsign;
	ix = hx & 0x7fff;
	if (ix >= BIAS + 13) {		/* |x| >= 8192 or x is NaN */
		if (ix == BIAS + LDBL_MAX_EXP) {
			if (hx & 0x8000 && u.xbits.man == 1ULL << 63)
				return (0.0L);	/* x is -Inf */
			return (x + x); /* x is +Inf, NaN or unsupported */
		}
		if (x > o_threshold.e)
			return (huge * huge);
		if (x < u_threshold.e)
			return (tiny * tiny);
	} else if (ix <= BIAS - 34) {	/* |x| < 0x1p-33 */
					/* includes pseudo-denormals */
	    	if (huge + x > 1.0L)	/* trigger inexact iff x != 0 */
			return (1.0L + x);
	}

	ENTERI();

	/* Reduce x to (k*ln2 + midpoint[n2] + r1 + r2). */
	/* Use a specialized rint() to get fn.  Assume round-to-nearest. */
	fn = x * INV_L + 0x1.8p63 - 0x1.8p63;
	r = x - fn * L1 - fn * L2;	/* r = r1 + r2 done independently. */
#if defined(HAVE_EFFICIENT_IRINTL)
	n  = irintl(fn);
#elif defined(HAVE_EFFICIENT_IRINT)
	n  = irint(fn);
#else
	n  = (int)fn;
#endif
	n2 = (unsigned)n % INTERVALS;		/* Tang's j. */
	k = (n - n2) / INTERVALS;
	r1 = x - fn * L1;
	r2 = -fn * L2;

	/* Prepare scale factors. */
	v.xbits.man = 1ULL << 63;
	if (k >= LDBL_MIN_EXP) {
		v.xbits.expsign = BIAS + k;
		twopk = v.e;
	} else {
		v.xbits.expsign = BIAS + k + 10000;
		twopkp10000 = v.e;
	}

	/* Evaluate expl(midpoint[n2] + r1 + r2) = s[n2] * expl(r1 + r2). */
	/* Here q = q(r), not q(r1), since r1 is lopped like L1. */
	t45 = r * P5 + P4;
	z = r * r;
	t23 = r * P3 + P2;
	q = r2 + z * t23 + z * z * t45 + z * z * z * P6;
	t = (long double)s[n2].lo + s[n2].hi;
	t = s[n2].lo + t * (q + r1) + s[n2].hi;

	/* Scale by 2**k. */
	if (k >= LDBL_MIN_EXP) {
		if (k == LDBL_MAX_EXP)
			RETURNI(t * 2.0L * 0x1p16383L);
		RETURNI(t * twopk);
	} else {
		RETURNI(t * twopkp10000 * twom10000);
	}
}
