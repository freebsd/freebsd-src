/*
 * Single-precision vector exp(x) - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f32.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float32x4_t poly[5];
  float32x4_t invln2_and_ln2;
  float32x4_t shift;
  int32x4_t exponent_bias;
#if WANT_SIMD_EXCEPT
  uint32x4_t thresh;
#else
  float32x4_t oflow_bound;
#endif
} data = {
  /* Generated using fpminimax with degree=5 in [-log(2)/2, log(2)/2].  */
  .poly = { V4 (0x1.fffffep-2), V4 (0x1.5554aep-3), V4 (0x1.555736p-5),
	    V4 (0x1.12287cp-7), V4 (0x1.6b55a2p-10) },
  /* Stores constants: invln2, ln2_hi, ln2_lo, 0.  */
  .invln2_and_ln2 = { 0x1.715476p+0f, 0x1.62e4p-1f, 0x1.7f7d1cp-20f, 0 },
  .shift = V4 (0x1.8p23f),
  .exponent_bias = V4 (0x3f800000),
#if !WANT_SIMD_EXCEPT
  /* Value above which expm1f(x) should overflow. Absolute value of the
     underflow bound is greater than this, so it catches both cases - there is
     a small window where fallbacks are triggered unnecessarily.  */
  .oflow_bound = V4 (0x1.5ebc4p+6),
#else
  /* asuint(oflow_bound) - asuint(0x1p-23), shifted left by 1 for absolute
     compare.  */
  .thresh = V4 (0x1d5ebc40),
#endif
};

/* asuint(0x1p-23), shifted by 1 for abs compare.  */
#define TinyBound v_u32 (0x34000000 << 1)

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t special)
{
  return v_call_f32 (expm1f, x, y, special);
}

/* Single-precision vector exp(x) - 1 function.
   The maximum error is 1.51 ULP:
   _ZGVnN4v_expm1f (0x1.8baa96p-2) got 0x1.e2fb9p-2
				  want 0x1.e2fb94p-2.  */
float32x4_t VPCS_ATTR V_NAME_F1 (expm1) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t ix = vreinterpretq_u32_f32 (x);

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to scalar for
     |x| < 2^-23, |x| > oflow_bound, Inf & NaN. Add ix to itself for
     shift-left by 1, and compare with thresh which was left-shifted offline -
     this is effectively an absolute compare.  */
  uint32x4_t special
      = vcgeq_u32 (vsubq_u32 (vaddq_u32 (ix, ix), TinyBound), d->thresh);
  if (unlikely (v_any_u32 (special)))
    x = v_zerofy_f32 (x, special);
#else
  /* Handles very large values (+ve and -ve), +/-NaN, +/-Inf.  */
  uint32x4_t special = vcagtq_f32 (x, d->oflow_bound);
#endif

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float32x4_t j = vsubq_f32 (
      vfmaq_laneq_f32 (d->shift, x, d->invln2_and_ln2, 0), d->shift);
  int32x4_t i = vcvtq_s32_f32 (j);
  float32x4_t f = vfmsq_laneq_f32 (x, j, d->invln2_and_ln2, 1);
  f = vfmsq_laneq_f32 (f, j, d->invln2_and_ln2, 2);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  float32x4_t p = v_horner_4_f32 (f, d->poly);
  p = vfmaq_f32 (f, vmulq_f32 (f, f), p);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  int32x4_t u = vaddq_s32 (vshlq_n_s32 (i, 23), d->exponent_bias);
  float32x4_t t = vreinterpretq_f32_s32 (u);

  if (unlikely (v_any_u32 (special)))
    return special_case (vreinterpretq_f32_u32 (ix),
			 vfmaq_f32 (vsubq_f32 (t, v_f32 (1.0f)), p, t),
			 special);

  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f32 (vsubq_f32 (t, v_f32 (1.0f)), p, t);
}

PL_SIG (V, F, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (V_NAME_F1 (expm1), 1.02)
PL_TEST_EXPECT_FENV (V_NAME_F1 (expm1), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (expm1), 0, 0x1p-23, 1000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), -0x1p-23, 0x1.5ebc4p+6, 1000000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), -0x1p-23, -0x1.9bbabcp+6, 1000000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), 0x1.5ebc4p+6, inf, 1000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), -0x1.9bbabcp+6, -inf, 1000)
