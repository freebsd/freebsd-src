/*
 * Single-precision e^x - 1 function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f32.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

#define Shift (0x1.8p23f)
#define InvLn2 (0x1.715476p+0f)
#define Ln2hi (0x1.62e4p-1f)
#define Ln2lo (0x1.7f7d1cp-20f)
#define AbsMask (0x7fffffff)
#define InfLimit                                                              \
  (0x1.644716p6) /* Smallest value of x for which expm1(x) overflows.  */
#define NegLimit                                                              \
  (-0x1.9bbabcp+6) /* Largest value of x for which expm1(x) rounds to 1.  */

/* Approximation for exp(x) - 1 using polynomial on a reduced interval.
   The maximum error is 1.51 ULP:
   expm1f(0x1.8baa96p-2) got 0x1.e2fb9p-2
			want 0x1.e2fb94p-2.  */
float
expm1f (float x)
{
  uint32_t ix = asuint (x);
  uint32_t ax = ix & AbsMask;

  /* Tiny: |x| < 0x1p-23. expm1(x) is closely approximated by x.
     Inf:  x == +Inf => expm1(x) = x.  */
  if (ax <= 0x34000000 || (ix == 0x7f800000))
    return x;

  /* +/-NaN.  */
  if (ax > 0x7f800000)
    return __math_invalidf (x);

  if (x >= InfLimit)
    return __math_oflowf (0);

  if (x <= NegLimit || ix == 0xff800000)
    return -1;

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float j = fmaf (InvLn2, x, Shift) - Shift;
  int32_t i = j;
  float f = fmaf (j, -Ln2hi, x);
  f = fmaf (j, -Ln2lo, f);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  float p = fmaf (f * f, horner_4_f32 (f, __expm1f_poly), f);
  /* Assemble the result, using a slight rearrangement to achieve acceptable
     accuracy.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^(i - 1).  */
  float t = ldexpf (0.5f, i);
  /* expm1(x) ~= 2 * (p * t + (t - 1/2)).  */
  return 2 * fmaf (p, t, t - 0.5f);
}

TEST_SIG (S, F, 1, expm1, -9.9, 9.9)
TEST_ULP (expm1f, 1.02)
TEST_SYM_INTERVAL (expm1f, 0, 0x1p-23, 1000)
TEST_INTERVAL (expm1f, 0x1p-23, 0x1.644716p6, 100000)
TEST_INTERVAL (expm1f, 0x1.644716p6, inf, 1000)
TEST_INTERVAL (expm1f, -0x1p-23, -0x1.9bbabcp+6, 100000)
TEST_INTERVAL (expm1f, -0x1.9bbabcp+6, -inf, 1000)
