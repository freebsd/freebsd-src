/*
 * Double-precision vector tanh(x) function.
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f64.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float64x2_t poly[11];
  float64x2_t inv_ln2, ln2_hi, ln2_lo, shift;
  uint64x2_t onef;
  uint64x2_t thresh, tiny_bound;
} data = {
  /* Generated using Remez, deg=12 in [-log(2)/2, log(2)/2].  */
  .poly = { V2 (0x1p-1), V2 (0x1.5555555555559p-3), V2 (0x1.555555555554bp-5),
	    V2 (0x1.111111110f663p-7), V2 (0x1.6c16c16c1b5f3p-10),
	    V2 (0x1.a01a01affa35dp-13), V2 (0x1.a01a018b4ecbbp-16),
	    V2 (0x1.71ddf82db5bb4p-19), V2 (0x1.27e517fc0d54bp-22),
	    V2 (0x1.af5eedae67435p-26), V2 (0x1.1f143d060a28ap-29), },

  .inv_ln2 = V2 (0x1.71547652b82fep0),
  .ln2_hi = V2 (-0x1.62e42fefa39efp-1),
  .ln2_lo = V2 (-0x1.abc9e3b39803fp-56),
  .shift = V2 (0x1.8p52),

  .onef = V2 (0x3ff0000000000000),
  .tiny_bound = V2 (0x3e40000000000000), /* asuint64 (0x1p-27).  */
  /* asuint64(0x1.241bf835f9d5fp+4) - asuint64(tiny_bound).  */
  .thresh = V2 (0x01f241bf835f9d5f),
};

static inline float64x2_t
expm1_inline (float64x2_t x, const struct data *d)
{
  /* Helper routine for calculating exp(x) - 1. Vector port of the helper from
     the scalar variant of tanh.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  float64x2_t j = vsubq_f64 (vfmaq_f64 (d->shift, d->inv_ln2, x), d->shift);
  int64x2_t i = vcvtq_s64_f64 (j);
  float64x2_t f = vfmaq_f64 (x, j, d->ln2_hi);
  f = vfmaq_f64 (f, j, d->ln2_lo);

  /* Approximate expm1(f) using polynomial.  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t f4 = vmulq_f64 (f2, f2);
  float64x2_t p = vfmaq_f64 (
      f, f2, v_estrin_10_f64 (f, f2, f4, vmulq_f64 (f4, f4), d->poly));

  /* t = 2 ^ i.  */
  float64x2_t t = vreinterpretq_f64_u64 (
      vaddq_u64 (vreinterpretq_u64_s64 (i << 52), d->onef));
  /* expm1(x) = p * t + (t - 1).  */
  return vfmaq_f64 (vsubq_f64 (t, v_f64 (1)), p, t);
}

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (tanh, x, y, special);
}

/* Vector approximation for double-precision tanh(x), using a simplified
   version of expm1. The greatest observed error is 2.77 ULP:
   _ZGVnN2v_tanh(-0x1.c4a4ca0f9f3b7p-3) got -0x1.bd6a21a163627p-3
				       want -0x1.bd6a21a163624p-3.  */
float64x2_t VPCS_ATTR V_NAME_D1 (tanh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint64x2_t ia = vreinterpretq_u64_f64 (vabsq_f64 (x));

  float64x2_t u = x;

  /* Trigger special-cases for tiny, boring and infinity/NaN.  */
  uint64x2_t special = vcgtq_u64 (vsubq_u64 (ia, d->tiny_bound), d->thresh);
#if WANT_SIMD_EXCEPT
  /* To trigger fp exceptions correctly, set special lanes to a neutral value.
     They will be fixed up later by the special-case handler.  */
  if (unlikely (v_any_u64 (special)))
    u = v_zerofy_f64 (u, special);
#endif

  u = vaddq_f64 (u, u);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  float64x2_t q = expm1_inline (u, d);
  float64x2_t qp2 = vaddq_f64 (q, v_f64 (2));

  if (unlikely (v_any_u64 (special)))
    return special_case (x, vdivq_f64 (q, qp2), special);
  return vdivq_f64 (q, qp2);
}

PL_SIG (V, D, 1, tanh, -10.0, 10.0)
PL_TEST_ULP (V_NAME_D1 (tanh), 2.27)
PL_TEST_EXPECT_FENV (V_NAME_D1 (tanh), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (tanh), 0, 0x1p-27, 5000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (tanh), 0x1p-27, 0x1.241bf835f9d5fp+4, 50000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (tanh), 0x1.241bf835f9d5fp+4, inf, 1000)
