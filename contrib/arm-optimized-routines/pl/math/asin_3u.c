/*
 * Double-precision asin(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f64.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask (0x7fffffffffffffff)
#define Half (0x3fe0000000000000)
#define One (0x3ff0000000000000)
#define PiOver2 (0x1.921fb54442d18p+0)
#define Small (0x3e50000000000000) /* 2^-26.  */
#define Small16 (0x3e50)
#define QNaN (0x7ff8)

/* Fast implementation of double-precision asin(x) based on polynomial
   approximation.

   For x < Small, approximate asin(x) by x. Small = 2^-26 for correct rounding.

   For x in [Small, 0.5], use an order 11 polynomial P such that the final
   approximation is an odd polynomial: asin(x) ~ x + x^3 P(x^2).

   The largest observed error in this region is 1.01 ulps,
   asin(0x1.da9735b5a9277p-2) got 0x1.ed78525a927efp-2
			     want 0x1.ed78525a927eep-2.

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

   The largest observed error in this region is 2.69 ulps,
   asin(0x1.044ac9819f573p-1) got 0x1.110d7e85fdd5p-1
			     want 0x1.110d7e85fdd53p-1.  */
double
asin (double x)
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
    return x;

  /* Evaluate polynomial Q(x) = y + y * z * P(z) with
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

  /* asin(|x|) = Q(|x|)         , for |x| < 0.5
	       = pi/2 - 2 Q(|x|), for |x| >= 0.5.  */
  double y = ax < 0.5 ? p : fma (-2.0, p, PiOver2);

  /* Copy sign.  */
  return asdouble (asuint64 (y) | sign);
}

PL_SIG (S, D, 1, asin, -1.0, 1.0)
PL_TEST_ULP (asin, 2.19)
PL_TEST_INTERVAL (asin, 0, Small, 5000)
PL_TEST_INTERVAL (asin, Small, 0.5, 50000)
PL_TEST_INTERVAL (asin, 0.5, 1.0, 50000)
PL_TEST_INTERVAL (asin, 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (asin, 0x1p11, inf, 20000)
PL_TEST_INTERVAL (asin, -0, -inf, 20000)
