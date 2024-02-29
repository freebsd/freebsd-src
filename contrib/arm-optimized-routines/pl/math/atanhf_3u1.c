/*
 * Single-precision atanh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffff
#define Half 0x3f000000
#define One 0x3f800000
#define Four 0x40800000
#define Ln2 0x1.62e43p-1f
/* asuint(0x1p-12), below which atanhf(x) rounds to x.  */
#define TinyBound 0x39800000

#define C(i) __log1pf_data.coeffs[i]

static inline float
eval_poly (float m)
{
  /* Approximate log(1+m) on [-0.25, 0.5] using Estrin scheme.  */
  float p_12 = fmaf (m, C (1), C (0));
  float p_34 = fmaf (m, C (3), C (2));
  float p_56 = fmaf (m, C (5), C (4));
  float p_78 = fmaf (m, C (7), C (6));

  float m2 = m * m;
  float p_02 = fmaf (m2, p_12, m);
  float p_36 = fmaf (m2, p_56, p_34);
  float p_79 = fmaf (m2, C (8), p_78);

  float m4 = m2 * m2;
  float p_06 = fmaf (m4, p_36, p_02);

  return fmaf (m4 * p_79, m4, p_06);
}

static inline float
log1pf_inline (float x)
{
  /* Helper for calculating log(x + 1). Copied from log1pf_2u1.c, with no
     special-case handling. See that file for details of the algorithm.  */
  float m = x + 1.0f;
  int k = (asuint (m) - 0x3f400000) & 0xff800000;
  float s = asfloat (Four - k);
  float m_scale = asfloat (asuint (x) - k) + fmaf (0.25f, s, -1.0f);
  float p = eval_poly (m_scale);
  float scale_back = (float) k * 0x1.0p-23f;
  return fmaf (scale_back, Ln2, p);
}

/* Approximation for single-precision inverse tanh(x), using a simplified
   version of log1p. Maximum error is 3.08 ULP:
   atanhf(0x1.ff0d5p-5) got 0x1.ffb768p-5
		       want 0x1.ffb76ep-5.  */
float
atanhf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t iax = ix & AbsMask;
  uint32_t sign = ix & ~AbsMask;

  if (unlikely (iax < TinyBound))
    return x;

  if (iax == One)
    return __math_divzero (sign);

  if (unlikely (iax > One))
    return __math_invalidf (x);

  float halfsign = asfloat (Half | sign);
  float ax = asfloat (iax);
  return halfsign * log1pf_inline ((2 * ax) / (1 - ax));
}

PL_SIG (S, F, 1, atanh, -1.0, 1.0)
PL_TEST_ULP (atanhf, 2.59)
PL_TEST_SYM_INTERVAL (atanhf, 0, 0x1p-12, 500)
PL_TEST_SYM_INTERVAL (atanhf, 0x1p-12, 1, 200000)
PL_TEST_SYM_INTERVAL (atanhf, 1, inf, 1000)
