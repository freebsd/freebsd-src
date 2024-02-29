/*
 * Double-precision vector exp(x) - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float64x2_t poly[11];
  float64x2_t invln2, ln2, shift;
  int64x2_t exponent_bias;
#if WANT_SIMD_EXCEPT
  uint64x2_t thresh, tiny_bound;
#else
  float64x2_t oflow_bound;
#endif
} data = {
  /* Generated using fpminimax, with degree=12 in [log(2)/2, log(2)/2].  */
  .poly = { V2 (0x1p-1), V2 (0x1.5555555555559p-3), V2 (0x1.555555555554bp-5),
	    V2 (0x1.111111110f663p-7), V2 (0x1.6c16c16c1b5f3p-10),
	    V2 (0x1.a01a01affa35dp-13), V2 (0x1.a01a018b4ecbbp-16),
	    V2 (0x1.71ddf82db5bb4p-19), V2 (0x1.27e517fc0d54bp-22),
	    V2 (0x1.af5eedae67435p-26), V2 (0x1.1f143d060a28ap-29) },
  .invln2 = V2 (0x1.71547652b82fep0),
  .ln2 = { 0x1.62e42fefa39efp-1, 0x1.abc9e3b39803fp-56 },
  .shift = V2 (0x1.8p52),
  .exponent_bias = V2 (0x3ff0000000000000),
#if WANT_SIMD_EXCEPT
  /* asuint64(oflow_bound) - asuint64(0x1p-51), shifted left by 1 for abs
     compare.  */
  .thresh = V2 (0x78c56fa6d34b552),
  /* asuint64(0x1p-51) << 1.  */
  .tiny_bound = V2 (0x3cc0000000000000 << 1),
#else
  /* Value above which expm1(x) should overflow. Absolute value of the
     underflow bound is greater than this, so it catches both cases - there is
     a small window where fallbacks are triggered unnecessarily.  */
  .oflow_bound = V2 (0x1.62b7d369a5aa9p+9),
#endif
};

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (expm1, x, y, special);
}

/* Double-precision vector exp(x) - 1 function.
   The maximum error observed error is 2.18 ULP:
   _ZGVnN2v_expm1 (0x1.634ba0c237d7bp-2) got 0x1.a8b9ea8d66e22p-2
					want 0x1.a8b9ea8d66e2p-2.  */
float64x2_t VPCS_ATTR V_NAME_D1 (expm1) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint64x2_t ix = vreinterpretq_u64_f64 (x);

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to scalar for
     |x| < 2^-51, |x| > oflow_bound, Inf & NaN. Add ix to itself for
     shift-left by 1, and compare with thresh which was left-shifted offline -
     this is effectively an absolute compare.  */
  uint64x2_t special
      = vcgeq_u64 (vsubq_u64 (vaddq_u64 (ix, ix), d->tiny_bound), d->thresh);
  if (unlikely (v_any_u64 (special)))
    x = v_zerofy_f64 (x, special);
#else
  /* Large input, NaNs and Infs.  */
  uint64x2_t special = vcageq_f64 (x, d->oflow_bound);
#endif

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float64x2_t n = vsubq_f64 (vfmaq_f64 (d->shift, d->invln2, x), d->shift);
  int64x2_t i = vcvtq_s64_f64 (n);
  float64x2_t f = vfmsq_laneq_f64 (x, n, d->ln2, 0);
  f = vfmsq_laneq_f64 (f, n, d->ln2, 1);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t f4 = vmulq_f64 (f2, f2);
  float64x2_t f8 = vmulq_f64 (f4, f4);
  float64x2_t p = vfmaq_f64 (f, f2, v_estrin_10_f64 (f, f2, f4, f8, d->poly));

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  int64x2_t u = vaddq_s64 (vshlq_n_s64 (i, 52), d->exponent_bias);
  float64x2_t t = vreinterpretq_f64_s64 (u);

  if (unlikely (v_any_u64 (special)))
    return special_case (vreinterpretq_f64_u64 (ix),
			 vfmaq_f64 (vsubq_f64 (t, v_f64 (1.0)), p, t),
			 special);

  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f64 (vsubq_f64 (t, v_f64 (1.0)), p, t);
}

PL_SIG (V, D, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (V_NAME_D1 (expm1), 1.68)
PL_TEST_EXPECT_FENV (V_NAME_D1 (expm1), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (expm1), 0, 0x1p-51, 1000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (expm1), 0x1p-51, 0x1.62b7d369a5aa9p+9, 100000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (expm1), 0x1.62b7d369a5aa9p+9, inf, 100)
