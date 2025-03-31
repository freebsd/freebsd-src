/*
 * Double-precision atan(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "test_sig.h"
#include "test_defs.h"
#include "atan_common.h"

#define AbsMask 0x7fffffffffffffff
#define PiOver2 0x1.921fb54442d18p+0
#define TinyBound 0x3e1 /* top12(asuint64(0x1p-30)).  */
#define BigBound 0x434	/* top12(asuint64(0x1p53)).  */
#define OneTop 0x3ff

/* Fast implementation of double-precision atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=1/x and shift = pi/2. Maximum observed error is 2.27 ulps:
   atan(0x1.0005af27c23e9p+0) got 0x1.9225645bdd7c1p-1
			     want 0x1.9225645bdd7c3p-1.  */
double
atan (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t sign = ix & ~AbsMask;
  uint64_t ia = ix & AbsMask;
  uint32_t ia12 = ia >> 52;

  if (unlikely (ia12 >= BigBound || ia12 < TinyBound))
    {
      if (ia12 < TinyBound)
	/* Avoid underflow by returning x.  */
	return x;
      if (ia > 0x7ff0000000000000)
	/* Propagate NaN.  */
	return __math_invalid (x);
      /* atan(x) rounds to PiOver2 for large x.  */
      return asdouble (asuint64 (PiOver2) ^ sign);
    }

  double z, az, shift;
  if (ia12 >= OneTop)
    {
      /* For x > 1, use atan(x) = pi / 2 + atan(-1 / x).  */
      z = -1.0 / x;
      shift = PiOver2;
      /* Use absolute value only when needed (odd powers of z).  */
      az = -fabs (z);
    }
  else
    {
      /* For x < 1, approximate atan(x) directly.  */
      z = x;
      shift = 0;
      az = asdouble (ia);
    }

  /* Calculate polynomial, shift + z + z^3 * P(z^2).  */
  double y = eval_poly (z, az, shift);
  /* Copy sign.  */
  return asdouble (asuint64 (y) ^ sign);
}

TEST_SIG (S, D, 1, atan, -10.0, 10.0)
TEST_ULP (atan, 1.78)
TEST_INTERVAL (atan, 0, 0x1p-30, 10000)
TEST_INTERVAL (atan, -0, -0x1p-30, 1000)
TEST_INTERVAL (atan, 0x1p-30, 0x1p53, 900000)
TEST_INTERVAL (atan, -0x1p-30, -0x1p53, 90000)
TEST_INTERVAL (atan, 0x1p53, inf, 10000)
TEST_INTERVAL (atan, -0x1p53, -inf, 1000)
