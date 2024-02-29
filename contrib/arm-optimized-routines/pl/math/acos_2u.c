/*
 * Double-precision acos(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "poly_scalar_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask (0x7fffffffffffffff)
#define Half (0x3fe0000000000000)
#define One (0x3ff0000000000000)
#define PiOver2 (0x1.921fb54442d18p+0)
#define Pi (0x1.921fb54442d18p+1)
#define Small (0x3c90000000000000) /* 2^-53.  */
#define Small16 (0x3c90)
#define QNaN (0x7ff8)

/* Fast implementation of double-precision acos(x) based on polynomial
   approximation of double-precision asin(x).

   For x < Small, approximate acos(x) by pi/2 - x. Small = 2^-53 for correct
   rounding.

   For |x| in [Small, 0.5], use the trigonometric identity

     acos(x) = pi/2 - asin(x)

   and use an order 11 polynomial P such that the final approximation of asin is
   an odd polynomial: asin(x) ~ x + x^3 * P(x^2).

   The largest observed error in this region is 1.18 ulps,
   acos(0x1.fbab0a7c460f6p-2) got 0x1.0d54d1985c068p+0
			     want 0x1.0d54d1985c069p+0.

   For |x| in [0.5, 1.0], use the following development of acos(x) near x = 1

     acos(x) ~ pi/2 - 2 * sqrt(z) (1 + z * P(z))

   where z = (1-x)/2, z is near 0 when x approaches 1, and P contributes to the
   approximation of asin near 0.

   The largest observed error in this region is 1.52 ulps,
   acos(0x1.23d362722f591p-1) got 0x1.edbbedf8a7d6ep-1
			     want 0x1.edbbedf8a7d6cp-1.

   For x in [-1.0, -0.5], use this other identity to deduce the negative inputs
   from their absolute value: acos(x) = pi - acos(-x).  */
double
acos (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t ia = ix & AbsMask;
  uint64_t ia16 = ia >> 48;
  double ax = asdouble (ia);
  uint64_t sign = ix & ~AbsMask;

  /* Special values and invalid range.  */
  if (unlikely (ia16 == QNaN))
    return x;
  if (ia > One)
    return __math_invalid (x);
  if (ia16 < Small16)
    return PiOver2 - x;

  /* Evaluate polynomial Q(|x|) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  double z2 = ax < 0.5 ? x * x : fma (-0.5, ax, 0.5);
  double z = ax < 0.5 ? ax : sqrt (z2);

  /* Use a single polynomial approximation P for both intervals.  */
  double z4 = z2 * z2;
  double z8 = z4 * z4;
  double z16 = z8 * z8;
  double p = estrin_11_f64 (z2, z4, z8, z16, __asin_poly);

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = fma (z * z2, p, z);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for |x| < 0.5
	       = pi - 2 Q(|x|), for -1.0 < x <= -0.5
	       = 2 Q(|x|)     , for -0.5 < x < 0.0.  */
  if (ax < 0.5)
    return PiOver2 - asdouble (asuint64 (p) | sign);

  return (x <= -0.5) ? fma (-2.0, p, Pi) : 2.0 * p;
}

PL_SIG (S, D, 1, acos, -1.0, 1.0)
PL_TEST_ULP (acos, 1.02)
PL_TEST_INTERVAL (acos, 0, Small, 5000)
PL_TEST_INTERVAL (acos, Small, 0.5, 50000)
PL_TEST_INTERVAL (acos, 0.5, 1.0, 50000)
PL_TEST_INTERVAL (acos, 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (acos, 0x1p11, inf, 20000)
PL_TEST_INTERVAL (acos, -0, -inf, 20000)
