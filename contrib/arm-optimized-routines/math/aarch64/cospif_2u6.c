/*
 * Single-precision scalar cospi function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

/* Taylor series coefficents for sin(pi * x).  */
#define C0 0x1.921fb6p1f
#define C1 -0x1.4abbcep2f
#define C2 0x1.466bc6p1f
#define C3 -0x1.32d2ccp-1f
#define C4 0x1.50783p-4f
#define C5 -0x1.e30750p-8f

#define Shift 0x1.0p+23f

/* Approximation for scalar single-precision cospi(x) - cospif.
   Maximum error: 2.64 ULP:
   cospif(0x1.37e844p-4) got 0x1.f16b3p-1
			want 0x1.f16b2ap-1.  */
float
arm_math_cospif (float x)
{
  if (isinf (x) || isnan (x))
    return __math_invalidf (x);

  float ax = asfloat (asuint (x) & ~0x80000000);

  /* Edge cases for when cospif should be exactly +/- 1. (Integers)
     0x1p23 is the limit for single precision to store any decimal places.  */
  if (ax >= 0x1p24f)
    return 1;

  uint32_t m = roundf (ax);
  if (m == ax)
    return (m & 1) ? -1 : 1;

  /* Any non-integer values >= 0x1p22f will be int +0.5.
     These values should return exactly 0.  */
  if (ax >= 0x1p22f)
    return 0;

  /* For very small inputs, squaring r causes underflow.
     Values below this threshold can be approximated via cospi(x) ~= 1 -
     (pi*x).  */
  if (ax < 0x1p-31f)
    return 1 - (C0 * x);

  /* n = rint(|x|).  */
  float n = ax + Shift;
  uint32_t sign = asuint (n) << 31;
  n = n - Shift;

  /* We know that cospi(x) = sinpi(0.5 - x)
     range reduction and offset into sinpi range -1/2 .. 1/2
     r = 0.5 - |x - rint(x)|.  */
  float r = 0.5f - fabs (ax - n);

  /* y = sin(pi * r).  */
  float r2 = r * r;
  float y = fmaf (C5, r2, C4);
  y = fmaf (y, r2, C3);
  y = fmaf (y, r2, C2);
  y = fmaf (y, r2, C1);
  y = fmaf (y, r2, C0);

  /* As all values are reduced to -1/2 .. 1/2, the result of cos(x) always be
     positive, therefore, the sign must be introduced based upon if x rounds to
     odd or even.  */
  return asfloat (asuint (y * r) ^ sign);
}

#if WANT_EXPERIMENTAL_MATH
float
cospif (float x)
{
  return arm_math_cospif (x);
}
#endif

#if WANT_TRIGPI_TESTS
TEST_ULP (arm_math_cospif, 2.15)
TEST_SYM_INTERVAL (arm_math_cospif, 0, 0x1p-31, 5000)
TEST_SYM_INTERVAL (arm_math_cospif, 0x1p-31, 0.5, 10000)
TEST_SYM_INTERVAL (arm_math_cospif, 0.5, 0x1p22f, 10000)
TEST_SYM_INTERVAL (arm_math_cospif, 0x1p22f, inf, 10000)
#endif
