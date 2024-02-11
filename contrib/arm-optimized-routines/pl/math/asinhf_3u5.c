/*
 * Single-precision asinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f32.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask (0x7fffffff)
#define SqrtFltMax (0x1.749e96p+10f)
#define Ln2 (0x1.62e4p-1f)
#define One (0x3f8)
#define ExpM12 (0x398)

float
optr_aor_log_f32 (float);

/* asinhf approximation using a variety of approaches on different intervals:

   |x| < 2^-12: Return x. Function is exactly rounded in this region.

   |x| < 1.0: Use custom order-8 polynomial. The largest observed
     error in this region is 1.3ulps:
     asinhf(0x1.f0f74cp-1) got 0x1.b88de4p-1 want 0x1.b88de2p-1.

   |x| <= SqrtFltMax: Calculate the result directly using the
     definition of asinh(x) = ln(x + sqrt(x*x + 1)). The largest
     observed error in this region is 1.99ulps.
     asinhf(0x1.00e358p+0) got 0x1.c4849ep-1 want 0x1.c484a2p-1.

   |x| > SqrtFltMax: We cannot square x without overflow at a low
     cost. At very large x, asinh(x) ~= ln(2x). At huge x we cannot
     even double x without overflow, so calculate this as ln(x) +
     ln(2). This largest observed error in this region is 3.39ulps.
     asinhf(0x1.749e9ep+10) got 0x1.fffff8p+2 want 0x1.fffffep+2.  */
float
asinhf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t ia = ix & AbsMask;
  uint32_t ia12 = ia >> 20;
  float ax = asfloat (ia);
  uint32_t sign = ix & ~AbsMask;

  if (unlikely (ia12 < ExpM12 || ia == 0x7f800000))
    return x;

  if (unlikely (ia12 >= 0x7f8))
    return __math_invalidf (x);

  if (ia12 < One)
    {
      float x2 = ax * ax;
      float p = estrin_7_f32 (ax, x2, x2 * x2, __asinhf_data.coeffs);
      float y = fmaf (x2, p, ax);
      return asfloat (asuint (y) | sign);
    }

  if (unlikely (ax > SqrtFltMax))
    {
      return asfloat (asuint (optr_aor_log_f32 (ax) + Ln2) | sign);
    }

  return asfloat (asuint (optr_aor_log_f32 (ax + sqrtf (ax * ax + 1))) | sign);
}

PL_SIG (S, F, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (asinhf, 2.9)
PL_TEST_INTERVAL (asinhf, 0, 0x1p-12, 5000)
PL_TEST_INTERVAL (asinhf, 0x1p-12, 1.0, 50000)
PL_TEST_INTERVAL (asinhf, 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (asinhf, 0x1p11, 0x1p127, 20000)
