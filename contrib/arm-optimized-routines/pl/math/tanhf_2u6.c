/*
 * Single-precision tanh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define BoringBound                                                            \
  0x41102cb3 /* 0x1.205966p+3, above which tanhf rounds to 1 (or -1 for        \
		negative).  */
#define AbsMask 0x7fffffff
#define One 0x3f800000

#define Shift (0x1.8p23f)
#define InvLn2 (0x1.715476p+0f)
#define Ln2hi (0x1.62e4p-1f)
#define Ln2lo (0x1.7f7d1cp-20f)

#define C(i) __expm1f_poly[i]

static inline float
expm1f_inline (float x)
{
  /* Helper routine for calculating exp(x) - 1.
     Copied from expm1f_1u6.c, with several simplifications:
     - No special-case handling for tiny or special values, instead return early
       from the main routine.
     - No special handling for large values:
       - No early return for infinity.
       - Simpler combination of p and t in final stage of algorithm.
       - |i| < 27, so can calculate t by simpler shift-and-add, instead of
	 ldexpf (same as vector algorithm).  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  float j = fmaf (InvLn2, x, Shift) - Shift;
  int32_t i = j;
  float f = fmaf (j, -Ln2hi, x);
  f = fmaf (j, -Ln2lo, f);

  /* Approximate expm1(f) with polynomial P, expm1(f) ~= f + f^2 * P(f).
     Uses Estrin scheme, where the main expm1f routine uses Horner.  */
  float f2 = f * f;
  float p_01 = fmaf (f, C (1), C (0));
  float p_23 = fmaf (f, C (3), C (2));
  float p = fmaf (f2, p_23, p_01);
  p = fmaf (f2 * f2, C (4), p);
  p = fmaf (f2, p, f);

  /* t = 2^i.  */
  float t = asfloat ((uint32_t) (i + 127) << 23);
  /* expm1(x) ~= p * t + (t - 1).  */
  return fmaf (p, t, t - 1);
}

/* Approximation for single-precision tanh(x), using a simplified version of
   expm1f. The maximum error is 2.58 ULP:
   tanhf(0x1.fa5eep-5) got 0x1.f9ba02p-5
		      want 0x1.f9ba08p-5.  */
float
tanhf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t iax = ix & AbsMask;
  uint32_t sign = ix & ~AbsMask;

  if (unlikely (iax > BoringBound))
    {
      if (iax > 0x7f800000)
	return __math_invalidf (x);
      return asfloat (One | sign);
    }

  if (unlikely (iax < 0x34000000))
    return x;

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  float q = expm1f_inline (2 * x);
  return q / (q + 2);
}

PL_SIG (S, F, 1, tanh, -10.0, 10.0)
PL_TEST_ULP (tanhf, 2.09)
PL_TEST_SYM_INTERVAL (tanhf, 0, 0x1p-23, 1000)
PL_TEST_SYM_INTERVAL (tanhf, 0x1p-23, 0x1.205966p+3, 100000)
PL_TEST_SYM_INTERVAL (tanhf, 0x1.205966p+3, inf, 100)
