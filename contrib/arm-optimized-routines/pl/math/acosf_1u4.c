/*
 * Single-precision acos(x) function.
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
#define Pif (0x1.921fb6p+1f)
#define Small (0x32800000) /* 2^-26.  */
#define Small12 (0x328)
#define QNaN (0x7fc)

/* Fast implementation of single-precision acos(x) based on polynomial
   approximation of single-precision asin(x).

   For x < Small, approximate acos(x) by pi/2 - x. Small = 2^-26 for correct
   rounding.

   For |x| in [Small, 0.5], use the trigonometric identity

     acos(x) = pi/2 - asin(x)

   and use an order 4 polynomial P such that the final approximation of asin is
   an odd polynomial: asin(x) ~ x + x^3 * P(x^2).

   The largest observed error in this region is 1.16 ulps,
     acosf(0x1.ffbeccp-2) got 0x1.0c27f8p+0 want 0x1.0c27f6p+0.

   For |x| in [0.5, 1.0], use the following development of acos(x) near x = 1

     acos(x) ~ pi/2 - 2 * sqrt(z) (1 + z * P(z))

   where z = (1-x)/2, z is near 0 when x approaches 1, and P contributes to the
   approximation of asin near 0.

   The largest observed error in this region is 1.32 ulps,
     acosf(0x1.15ba56p-1) got 0x1.feb33p-1 want 0x1.feb32ep-1.

   For x in [-1.0, -0.5], use this other identity to deduce the negative inputs
   from their absolute value.

     acos(x) = pi - acos(-x)

   The largest observed error in this region is 1.28 ulps,
     acosf(-0x1.002072p-1) got 0x1.0c1e84p+1 want 0x1.0c1e82p+1.  */
float
acosf (float x)
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
    return PiOver2f - x;

  /* Evaluate polynomial Q(|x|) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float z2 = ax < 0.5 ? x * x : fmaf (-0.5f, ax, 0.5f);
  float z = ax < 0.5 ? ax : sqrtf (z2);

  /* Use a single polynomial approximation P for both intervals.  */
  float p = horner_4_f32 (z2, __asinf_poly);
  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = fmaf (z * z2, p, z);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for |x| < 0.5
	       = pi - 2 Q(|x|), for -1.0 < x <= -0.5
	       = 2 Q(|x|)     , for -0.5 < x < 0.0.  */
  if (ax < 0.5)
    return PiOver2f - asfloat (asuint (p) | sign);

  return (x <= -0.5) ? fmaf (-2.0f, p, Pif) : 2.0f * p;
}

PL_SIG (S, F, 1, acos, -1.0, 1.0)
PL_TEST_ULP (acosf, 0.82)
PL_TEST_INTERVAL (acosf, 0, Small, 5000)
PL_TEST_INTERVAL (acosf, Small, 0.5, 50000)
PL_TEST_INTERVAL (acosf, 0.5, 1.0, 50000)
PL_TEST_INTERVAL (acosf, 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (acosf, 0x1p11, inf, 20000)
PL_TEST_INTERVAL (acosf, -0, -inf, 20000)
