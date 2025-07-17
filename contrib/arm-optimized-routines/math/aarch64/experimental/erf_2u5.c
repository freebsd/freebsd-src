/*
 * Double-precision erf(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

#define TwoOverSqrtPiMinusOne 0x1.06eba8214db69p-3
#define Shift 0x1p45

/* Polynomial coefficients.  */
#define OneThird 0x1.5555555555555p-2
#define TwoThird 0x1.5555555555555p-1

#define TwoOverFifteen 0x1.1111111111111p-3
#define TwoOverFive 0x1.999999999999ap-2
#define Tenth 0x1.999999999999ap-4

#define TwoOverNine 0x1.c71c71c71c71cp-3
#define TwoOverFortyFive 0x1.6c16c16c16c17p-5
#define Sixth 0x1.555555555555p-3

/* Fast erf approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r)
     + scale * d * [
       + 1
       - r d
       + 1/3 (2 r^2 - 1) d^2
       - 1/6 (r (2 r^2 - 3)) d^3
       + 1/30 (4 r^4 - 12 r^2 + 3) d^4
       - 1/90 (4 r^4 - 20 r^2 + 15) d^5
     ]

   Maximum measure error: 2.29 ULP
   erf(-0x1.00003c924e5d1p-8) got -0x1.20dd59132ebadp-8
			     want -0x1.20dd59132ebafp-8.  */
double
arm_math_erf (double x)
{
  /* Get absolute value and sign.  */
  uint64_t ix = asuint64 (x);
  uint64_t ia = ix & 0x7fffffffffffffff;
  uint64_t sign = ix & ~0x7fffffffffffffff;

  /* |x| < 0x1p-508. Triggers exceptions.  */
  if (unlikely (ia < 0x2030000000000000))
    return fma (TwoOverSqrtPiMinusOne, x, x);

  if (ia < 0x4017f80000000000) /* |x| <  6 - 1 / 128 = 5.9921875.  */
    {
      /* Set r to multiple of 1/128 nearest to |x|.  */
      double a = asdouble (ia);
      double z = a + Shift;
      uint64_t i = asuint64 (z) - asuint64 (Shift);
      double r = z - Shift;
      /* Lookup erf(r) and scale(r) in table.
	 Set erf(r) to 0 and scale to 2/sqrt(pi) for |x| <= 0x1.cp-9.  */
      double erfr = __v_erf_data.tab[i].erf;
      double scale = __v_erf_data.tab[i].scale;

      /* erf(x) ~ erf(r) + scale * d * poly (d, r).  */
      double d = a - r;
      double r2 = r * r;
      double d2 = d * d;

      /* poly (d, r) = 1 + p1(r) * d + p2(r) * d^2 + ... + p5(r) * d^5.  */
      double p1 = -r;
      double p2 = fma (TwoThird, r2, -OneThird);
      double p3 = -r * fma (OneThird, r2, -0.5);
      double p4 = fma (fma (TwoOverFifteen, r2, -TwoOverFive), r2, Tenth);
      double p5
	  = -r * fma (fma (TwoOverFortyFive, r2, -TwoOverNine), r2, Sixth);

      double p34 = fma (p4, d, p3);
      double p12 = fma (p2, d, p1);
      double y = fma (p5, d2, p34);
      y = fma (y, d2, p12);

      y = fma (fma (y, d2, d), scale, erfr);
      return asdouble (asuint64 (y) | sign);
    }

  /* Special cases : erf(nan)=nan, erf(+inf)=+1 and erf(-inf)=-1.  */
  if (unlikely (ia >= 0x7ff0000000000000))
    return (1.0 - (double) (sign >> 62)) + 1.0 / x;

  /* Boring domain (|x| >= 6.0).  */
  return asdouble (sign | asuint64 (1.0));
}

TEST_ULP (arm_math_erf, 1.79)
TEST_SYM_INTERVAL (arm_math_erf, 0, 5.9921875, 40000)
TEST_SYM_INTERVAL (arm_math_erf, 5.9921875, inf, 40000)
TEST_SYM_INTERVAL (arm_math_erf, 0, inf, 40000)
