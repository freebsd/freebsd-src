/*
 * Double-precision erfc(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define Shift 0x1p45
#define P20 0x1.5555555555555p-2 /* 1/3.  */
#define P21 0x1.5555555555555p-1 /* 2/3.  */

#define P40 0x1.999999999999ap-4  /* 1/10.  */
#define P41 0x1.999999999999ap-2  /* 2/5.  */
#define P42 0x1.11111111111111p-3 /* 2/15.  */

#define P50 0x1.5555555555555p-3 /* 1/6.  */
#define P51 0x1.c71c71c71c71cp-3 /* 2/9.  */
#define P52 0x1.6c16c16c16c17p-5 /* 2/45.  */

/* Qi = (i+1) / i.  */
#define Q5 0x1.3333333333333p0
#define Q6 0x1.2aaaaaaaaaaabp0
#define Q7 0x1.2492492492492p0
#define Q8 0x1.2p0
#define Q9 0x1.1c71c71c71c72p0

/* Ri = -2 * i / ((i+1)*(i+2)).  */
#define R5 -0x1.e79e79e79e79ep-3
#define R6 -0x1.b6db6db6db6dbp-3
#define R7 -0x1.8e38e38e38e39p-3
#define R8 -0x1.6c16c16c16c17p-3
#define R9 -0x1.4f2094f2094f2p-3

/* Fast erfc approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4
		- r * (2/45 r^4 - 2/9 r^2 + 1/6) d^5
		+ p6(r) d^6 + ... + p10(r) d^10

   Polynomials p6(r) to p10(r) are computed using recurrence relation

   2(i+1)p_i + 2r(i+2)p_{i+1} + (i+2)(i+3)p_{i+2} = 0,
   with p0 = 1, and p1(r) = -r.

   Values of erfc(r) and scale(r) are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).

   Maximum measured error: 1.71 ULP
   erfc(0x1.46cfe976733p+4) got 0x1.e15fcbea3e7afp-608
			   want 0x1.e15fcbea3e7adp-608.  */
double
erfc (double x)
{
  /* Get top words and sign.  */
  uint64_t ix = asuint64 (x);
  uint64_t ia = ix & 0x7fffffffffffffff;
  double a = asdouble (ia);
  uint64_t sign = ix & ~0x7fffffffffffffff;

  /* erfc(nan)=nan, erfc(+inf)=0 and erfc(-inf)=2.  */
  if (unlikely (ia >= 0x7ff0000000000000))
    return asdouble (sign >> 1) + 1.0 / x; /* Special cases.  */

  /* Return early for large enough negative values.  */
  if (x < -6.0)
    return 2.0;

  /* For |x| < 3487.0/128.0, the following approximation holds.  */
  if (likely (ia < 0x403b3e0000000000))
    {
      /* |x| < 0x1p-511 => accurate to 0.5 ULP.  */
      if (unlikely (ia < asuint64 (0x1p-511)))
	return 1.0 - x;

      /* Lookup erfc(r) and scale(r) in tables, e.g. set erfc(r) to 1 and scale
	 to 2/sqrt(pi), when x reduced to r = 0.  */
      double z = a + Shift;
      uint64_t i = asuint64 (z);
      double r = z - Shift;
      /* These values are scaled by 2^128.  */
      double erfcr = __erfc_data.tab[i].erfc;
      double scale = __erfc_data.tab[i].scale;

      /* erfc(x) ~ erfc(r) - scale * d * poly (r, d).  */
      double d = a - r;
      double d2 = d * d;
      double r2 = r * r;
      /* Compute p_i as a regular (low-order) polynomial.  */
      double p1 = -r;
      double p2 = fma (P21, r2, -P20);
      double p3 = -r * fma (P20, r2, -0.5);
      double p4 = fma (fma (P42, r2, -P41), r2, P40);
      double p5 = -r * fma (fma (P52, r2, -P51), r2, P50);
      /* Compute p_i using recurrence relation:
	 p_{i+2} = (p_i + r * Q_{i+1} * p_{i+1}) * R_{i+1}.  */
      double p6 = fma (Q5 * r, p5, p4) * R5;
      double p7 = fma (Q6 * r, p6, p5) * R6;
      double p8 = fma (Q7 * r, p7, p6) * R7;
      double p9 = fma (Q8 * r, p8, p7) * R8;
      double p10 = fma (Q9 * r, p9, p8) * R9;
      /* Compute polynomial in d using pairwise Horner scheme.  */
      double p90 = fma (p10, d, p9);
      double p78 = fma (p8, d, p7);
      double p56 = fma (p6, d, p5);
      double p34 = fma (p4, d, p3);
      double p12 = fma (p2, d, p1);
      double y = fma (p90, d2, p78);
      y = fma (y, d2, p56);
      y = fma (y, d2, p34);
      y = fma (y, d2, p12);

      y = fma (-fma (y, d2, d), scale, erfcr);

      /* Handle sign and scale back in a single fma.  */
      double off = asdouble (sign >> 1);
      double fac = asdouble (asuint64 (0x1p-128) | sign);
      y = fma (y, fac, off);

      if (unlikely (x > 26.0))
	{
	  /* The underflow exception needs to be signaled explicitly when
	     result gets into the subnormal range.  */
	  if (unlikely (y < 0x1p-1022))
	    force_eval_double (opt_barrier_double (0x1p-1022) * 0x1p-1022);
	  /* Set errno to ERANGE if result rounds to 0.  */
	  return __math_check_uflow (y);
	}

      return y;
    }
  /* Above the threshold (x > 3487.0/128.0) erfc is constant and needs to raise
     underflow exception for positive x.  */
  return __math_uflow (0);
}

PL_SIG (S, D, 1, erfc, -6.0, 28.0)
PL_TEST_ULP (erfc, 1.21)
PL_TEST_SYM_INTERVAL (erfc, 0, 0x1p-26, 40000)
PL_TEST_INTERVAL (erfc, 0x1p-26, 28.0, 100000)
PL_TEST_INTERVAL (erfc, -0x1p-26, -6.0, 100000)
PL_TEST_INTERVAL (erfc, 28.0, inf, 40000)
PL_TEST_INTERVAL (erfc, -6.0, -inf, 40000)
