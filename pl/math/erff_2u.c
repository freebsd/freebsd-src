/*
 * Single-precision erf(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define TwoOverSqrtPiMinusOne 0x1.06eba8p-3f
#define Shift 0x1p16f
#define OneThird 0x1.555556p-2f

/* Fast erff approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r)
     + scale * d * [
       + 1
       - r d
       + 1/3 (2 r^2 - 1) d^2
       - 1/6 (r (2 r^2 - 3) ) d^3
       + 1/30 (4 r^4 - 12 r^2 + 3) d^4
     ]

   This single precision implementation uses only the following terms:

   erf(x) ~ erf(r) + scale * d * [1 - r * d - 1/3 * d^2]

   Values of erf(r) and scale are read from lookup tables.
   For |x| > 3.9375, erf(|x|) rounds to 1.0f.

   Maximum error: 1.93 ULP
   erff(0x1.c373e6p-9) got 0x1.fd686cp-9
		      want 0x1.fd6868p-9.  */
float
erff (float x)
{
  /* Get absolute value and sign.  */
  uint32_t ix = asuint (x);
  uint32_t ia = ix & 0x7fffffff;
  uint32_t sign = ix & ~0x7fffffff;

  /* |x| < 0x1p-62. Triggers exceptions.  */
  if (unlikely (ia < 0x20800000))
    return fmaf (TwoOverSqrtPiMinusOne, x, x);

  if (ia < 0x407b8000) /* |x| <  4 - 8 / 128 = 3.9375.  */
    {
      /* Lookup erf(r) and scale(r) in tables, e.g. set erf(r) to 0 and scale
	 to 2/sqrt(pi), when x reduced to r = 0.  */
      float a = asfloat (ia);
      float z = a + Shift;
      uint32_t i = asuint (z) - asuint (Shift);
      float r = z - Shift;
      float erfr = __erff_data.tab[i].erf;
      float scale = __erff_data.tab[i].scale;

      /* erf(x) ~ erf(r) + scale * d * (1 - r * d - 1/3 * d^2).  */
      float d = a - r;
      float d2 = d * d;
      float y = -fmaf (OneThird, d, r);
      y = fmaf (fmaf (y, d2, d), scale, erfr);
      return asfloat (asuint (y) | sign);
    }

  /* Special cases : erff(nan)=nan, erff(+inf)=+1 and erff(-inf)=-1.  */
  if (unlikely (ia >= 0x7f800000))
    return (1.0f - (float) (sign >> 30)) + 1.0f / x;

  /* Boring domain (|x| >= 4.0).  */
  return asfloat (sign | asuint (1.0f));
}

PL_SIG (S, F, 1, erf, -4.0, 4.0)
PL_TEST_ULP (erff, 1.43)
PL_TEST_SYM_INTERVAL (erff, 0, 3.9375, 40000)
PL_TEST_SYM_INTERVAL (erff, 3.9375, inf, 40000)
PL_TEST_SYM_INTERVAL (erff, 0, inf, 40000)
