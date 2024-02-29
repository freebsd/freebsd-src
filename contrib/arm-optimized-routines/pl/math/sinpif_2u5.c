/*
 * Single-precision scalar sinpi function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

/* Taylor series coefficents for sin(pi * x).  */
#define C0 0x1.921fb6p1f
#define C1 -0x1.4abbcep2f
#define C2 0x1.466bc6p1f
#define C3 -0x1.32d2ccp-1f
#define C4 0x1.50783p-4f
#define C5 -0x1.e30750p-8f

#define Shift 0x1.0p+23f

/* Approximation for scalar single-precision sinpi(x) - sinpif.
   Maximum error: 2.48 ULP:
   sinpif(0x1.d062b6p-2) got 0x1.fa8c06p-1
			want 0x1.fa8c02p-1.  */
float
sinpif (float x)
{
  if (isinf (x))
    return __math_invalidf (x);

  float r = asfloat (asuint (x) & ~0x80000000);
  uint32_t sign = asuint (x) & 0x80000000;

  /* Edge cases for when sinpif should be exactly 0. (Integers)
     0x1p23 is the limit for single precision to store any decimal places.  */
  if (r >= 0x1p23f)
    return 0;

  int32_t m = roundf (r);
  if (m == r)
    return 0;

  /* For very small inputs, squaring r causes underflow.
     Values below this threshold can be approximated via sinpi(x) ~= pi*x.  */
  if (r < 0x1p-31f)
    return C0 * x;

  /* Any non-integer values >= 0x1p22f will be int + 0.5.
     These values should return exactly 1 or -1.  */
  if (r >= 0x1p22f)
    {
      uint32_t iy = ((m & 1) << 31) ^ asuint (-1.0f);
      return asfloat (sign ^ iy);
    }

  /* n = rint(|x|).  */
  float n = r + Shift;
  sign ^= (asuint (n) << 31);
  n = n - Shift;

  /* r = |x| - n (range reduction into -1/2 .. 1/2).  */
  r = r - n;

  /* y = sin(pi * r).  */
  float r2 = r * r;
  float y = fmaf (C5, r2, C4);
  y = fmaf (y, r2, C3);
  y = fmaf (y, r2, C2);
  y = fmaf (y, r2, C1);
  y = fmaf (y, r2, C0);

  /* Copy sign of x to sin(|x|).  */
  return asfloat (asuint (y * r) ^ sign);
}

PL_SIG (S, F, 1, sinpi, -0.9, 0.9)
PL_TEST_ULP (sinpif, 1.99)
PL_TEST_SYM_INTERVAL (sinpif, 0, 0x1p-31, 5000)
PL_TEST_SYM_INTERVAL (sinpif, 0x1p-31, 0.5, 10000)
PL_TEST_SYM_INTERVAL (sinpif, 0.5, 0x1p22f, 10000)
PL_TEST_SYM_INTERVAL (sinpif, 0x1p22f, inf, 10000)
