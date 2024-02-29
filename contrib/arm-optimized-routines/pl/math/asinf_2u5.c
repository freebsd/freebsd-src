/*
 * Single-precision asin(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f32.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask (0x7fffffff)
#define Half (0x3f000000)
#define One (0x3f800000)
#define PiOver2f (0x1.921fb6p+0f)
#define Small (0x39800000) /* 2^-12.  */
#define Small12 (0x398)
#define QNaN (0x7fc)

/* Fast implementation of single-precision asin(x) based on polynomial
   approximation.

   For x < Small, approximate asin(x) by x. Small = 2^-12 for correct rounding.

   For x in [Small, 0.5], use order 4 polynomial P such that the final
   approximation is an odd polynomial: asin(x) ~ x + x^3 P(x^2).

   The largest observed error in this region is 0.83 ulps,
     asinf(0x1.ea00f4p-2) got 0x1.fef15ep-2 want 0x1.fef15cp-2.

   No cheap approximation can be obtained near x = 1, since the function is not
   continuously differentiable on 1.

   For x in [0.5, 1.0], we use a method based on a trigonometric identity

     asin(x) = pi/2 - acos(x)

   and a generalized power series expansion of acos(y) near y=1, that reads as

     acos(y)/sqrt(2y) ~ 1 + 1/12 * y + 3/160 * y^2 + ... (1)

   The Taylor series of asin(z) near z = 0, reads as

     asin(z) ~ z + z^3 P(z^2) = z + z^3 * (1/6 + 3/40 z^2 + ...).

   Therefore, (1) can be written in terms of P(y/2) or even asin(y/2)

     acos(y) ~ sqrt(2y) (1 + y/2 * P(y/2)) = 2 * sqrt(y/2) (1 + y/2 * P(y/2)

   Hence, if we write z = (1-x)/2, z is near 0 when x approaches 1 and

     asin(x) ~ pi/2 - acos(x) ~ pi/2 - 2 * sqrt(z) (1 + z * P(z)).

   The largest observed error in this region is 2.41 ulps,
     asinf(0x1.00203ep-1) got 0x1.0c3a64p-1 want 0x1.0c3a6p-1.  */
float
asinf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t ia = ix & AbsMask;
  uint32_t ia12 = ia >> 20;
  float ax = asfloat (ia);
  uint32_t sign = ix & ~AbsMask;

  /* Special values and invalid range.  */
  if (unlikely (ia12 == QNaN))
    return x;
  if (ia > One)
    return __math_invalidf (x);
  if (ia12 < Small12)
    return x;

  /* Evaluate polynomial Q(x) = y + y * z * P(z) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float z2 = ax < 0.5 ? x * x : fmaf (-0.5f, ax, 0.5f);
  float z = ax < 0.5 ? ax : sqrtf (z2);

  /* Use a single polynomial approximation P for both intervals.  */
  float p = horner_4_f32 (z2, __asinf_poly);
  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = fmaf (z * z2, p, z);

  /* asin(|x|) = Q(|x|)         , for |x| < 0.5
	       = pi/2 - 2 Q(|x|), for |x| >= 0.5.  */
  float y = ax < 0.5 ? p : fmaf (-2.0f, p, PiOver2f);

  /* Copy sign.  */
  return asfloat (asuint (y) | sign);
}

PL_SIG (S, F, 1, asin, -1.0, 1.0)
PL_TEST_ULP (asinf, 1.91)
PL_TEST_INTERVAL (asinf, 0, Small, 5000)
PL_TEST_INTERVAL (asinf, Small, 0.5, 50000)
PL_TEST_INTERVAL (asinf, 0.5, 1.0, 50000)
PL_TEST_INTERVAL (asinf, 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (asinf, 0x1p11, inf, 20000)
PL_TEST_INTERVAL (asinf, -0, -inf, 20000)
