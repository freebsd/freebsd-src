/*
 * Single-precision cosh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffff
#define TinyBound 0x20000000 /* 0x1p-63: Round to 1 below this.  */
#define SpecialBound                                                           \
  0x42ad496c /* 0x1.5a92d8p+6: expf overflows above this, so have to use       \
		special case.  */

float
optr_aor_exp_f32 (float);

static NOINLINE float
specialcase (float x, uint32_t iax)
{
  if (iax == 0x7f800000)
    return INFINITY;
  if (iax > 0x7f800000)
    return __math_invalidf (x);
  if (iax <= TinyBound)
    /* For tiny x, avoid underflow by just returning 1.  */
    return 1;
  /* Otherwise SpecialBound <= |x| < Inf. x is too large to calculate exp(x)
     without overflow, so use exp(|x|/2) instead. For large x cosh(x) is
     dominated by exp(x), so return:
     cosh(x) ~= (exp(|x|/2))^2 / 2.  */
  float t = optr_aor_exp_f32 (asfloat (iax) / 2);
  return (0.5 * t) * t;
}

/* Approximation for single-precision cosh(x) using exp.
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The maximum error is 1.89 ULP, observed for |x| > SpecialBound:
   coshf(0x1.65898cp+6) got 0x1.f00aep+127 want 0x1.f00adcp+127.
   The maximum error observed for TinyBound < |x| < SpecialBound is 1.02 ULP:
   coshf(0x1.50a3cp+0) got 0x1.ff21dcp+0 want 0x1.ff21dap+0.  */
float
coshf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t iax = ix & AbsMask;
  float ax = asfloat (iax);

  if (unlikely (iax <= TinyBound || iax >= SpecialBound))
    {
      /* x is tiny, large or special.  */
      return specialcase (x, iax);
    }

  /* Compute cosh using the definition:
     coshf(x) = exp(x) / 2 + exp(-x) / 2.  */
  float t = optr_aor_exp_f32 (ax);
  return 0.5f * t + 0.5f / t;
}

PL_SIG (S, F, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (coshf, 1.89)
PL_TEST_INTERVAL (coshf, 0, 0x1p-63, 100)
PL_TEST_INTERVAL (coshf, 0, 0x1.5a92d8p+6, 80000)
PL_TEST_INTERVAL (coshf, 0x1.5a92d8p+6, inf, 2000)
PL_TEST_INTERVAL (coshf, -0, -0x1p-63, 100)
PL_TEST_INTERVAL (coshf, -0, -0x1.5a92d8p+6, 80000)
PL_TEST_INTERVAL (coshf, -0x1.5a92d8p+6, -inf, 2000)
