/*
 * Single-precision atan(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "atanf_common.h"
#include "test_sig.h"
#include "test_defs.h"

#define PiOver2 0x1.921fb6p+0f
#define AbsMask 0x7fffffff
#define TinyBound 0x30800000 /* asuint(0x1p-30).  */
#define BigBound 0x4e800000  /* asuint(0x1p30).  */
#define One 0x3f800000

/* Approximation of single-precision atan(x) based on
   atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1]
   using z=-1/x and shift = pi/2.
   Maximum error is 2.88 ulps:
   atanf(0x1.0565ccp+0) got 0x1.97771p-1
		       want 0x1.97770ap-1.  */
float
atanf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t sign = ix & ~AbsMask;
  uint32_t ia = ix & AbsMask;

  if (unlikely (ia < TinyBound))
    /* Avoid underflow by returning x.  */
    return x;

  if (unlikely (ia > BigBound))
    {
      if (ia > 0x7f800000)
	/* Propagate NaN.  */
	return __math_invalidf (x);
      /* atan(x) rounds to PiOver2 for large x.  */
      return asfloat (asuint (PiOver2) ^ sign);
    }

  float z, az, shift;
  if (ia > One)
    {
      /* For x > 1, use atan(x) = pi / 2 + atan(-1 / x).  */
      z = -1.0f / x;
      shift = PiOver2;
      /* Use absolute value only when needed (odd powers of z).  */
      az = -fabsf (z);
    }
  else
    {
      /* For x < 1, approximate atan(x) directly.  */
      z = x;
      az = asfloat (ia);
      shift = 0;
    }

  /* Calculate polynomial, shift + z + z^3 * P(z^2).  */
  float y = eval_poly (z, az, shift);
  /* Copy sign.  */
  return asfloat (asuint (y) ^ sign);
}

TEST_SIG (S, F, 1, atan, -10.0, 10.0)
TEST_ULP (atanf, 2.38)
TEST_SYM_INTERVAL (atanf, 0, 0x1p-30, 5000)
TEST_SYM_INTERVAL (atanf, 0x1p-30, 1, 40000)
TEST_SYM_INTERVAL (atanf, 1, 0x1p30, 40000)
TEST_SYM_INTERVAL (atanf, 0x1p30, inf, 1000)
