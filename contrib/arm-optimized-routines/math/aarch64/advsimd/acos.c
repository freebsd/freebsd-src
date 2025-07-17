/*
 * Double-precision vector acos(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "v_poly_f64.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float64x2_t poly[12];
  float64x2_t pi, pi_over_2;
  uint64x2_t abs_mask;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  .poly = { V2 (0x1.555555555554ep-3), V2 (0x1.3333333337233p-4),
	    V2 (0x1.6db6db67f6d9fp-5), V2 (0x1.f1c71fbd29fbbp-6),
	    V2 (0x1.6e8b264d467d6p-6), V2 (0x1.1c5997c357e9dp-6),
	    V2 (0x1.c86a22cd9389dp-7), V2 (0x1.856073c22ebbep-7),
	    V2 (0x1.fd1151acb6bedp-8), V2 (0x1.087182f799c1dp-6),
	    V2 (-0x1.6602748120927p-7), V2 (0x1.cfa0dd1f9478p-6), },
  .pi = V2 (0x1.921fb54442d18p+1),
  .pi_over_2 = V2 (0x1.921fb54442d18p+0),
  .abs_mask = V2 (0x7fffffffffffffff),
};

#define AllMask v_u64 (0xffffffffffffffff)
#define Oneu 0x3ff0000000000000
#define Small 0x3e50000000000000 /* 2^-53.  */

#if WANT_SIMD_EXCEPT
static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (acos, x, y, special);
}
#endif

/* Double-precision implementation of vector acos(x).

   For |x| < Small, approximate acos(x) by pi/2 - x. Small = 2^-53 for correct
   rounding.
   If WANT_SIMD_EXCEPT = 0, Small = 0 and we proceed with the following
   approximation.

   For |x| in [Small, 0.5], use an order 11 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

   The largest observed error in this region is 1.18 ulps,
   _ZGVnN2v_acos (0x1.fbab0a7c460f6p-2) got 0x1.0d54d1985c068p+0
				       want 0x1.0d54d1985c069p+0.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 1.52 ulps,
   _ZGVnN2v_acos (0x1.23d362722f591p-1) got 0x1.edbbedf8a7d6ep-1
				       want 0x1.edbbedf8a7d6cp-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (acos) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);

#if WANT_SIMD_EXCEPT
  /* A single comparison for One, Small and QNaN.  */
  uint64x2_t special
      = vcgtq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (ax), v_u64 (Small)),
		   v_u64 (Oneu - Small));
  if (unlikely (v_any_u64 (special)))
    return special_case (x, x, AllMask);
#endif

  uint64x2_t a_le_half = vcleq_f64 (ax, v_f64 (0.5));

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float64x2_t z2 = vbslq_f64 (a_le_half, vmulq_f64 (x, x),
			      vfmaq_f64 (v_f64 (0.5), v_f64 (-0.5), ax));
  float64x2_t z = vbslq_f64 (a_le_half, ax, vsqrtq_f64 (z2));

  /* Use a single polynomial approximation P for both intervals.  */
  float64x2_t z4 = vmulq_f64 (z2, z2);
  float64x2_t z8 = vmulq_f64 (z4, z4);
  float64x2_t z16 = vmulq_f64 (z8, z8);
  float64x2_t p = v_estrin_11_f64 (z2, z4, z8, z16, d->poly);

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = vfmaq_f64 (z, vmulq_f64 (z, z2), p);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  float64x2_t y = vbslq_f64 (d->abs_mask, p, x);

  uint64x2_t is_neg = vcltzq_f64 (x);
  float64x2_t off = vreinterpretq_f64_u64 (
      vandq_u64 (is_neg, vreinterpretq_u64_f64 (d->pi)));
  float64x2_t mul = vbslq_f64 (a_le_half, v_f64 (-1.0), v_f64 (2.0));
  float64x2_t add = vbslq_f64 (a_le_half, d->pi_over_2, off);

  return vfmaq_f64 (add, mul, y);
}

TEST_SIG (V, D, 1, acos, -1.0, 1.0)
TEST_ULP (V_NAME_D1 (acos), 1.02)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (acos), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_D1 (acos), 0, Small, 5000)
TEST_INTERVAL (V_NAME_D1 (acos), Small, 0.5, 50000)
TEST_INTERVAL (V_NAME_D1 (acos), 0.5, 1.0, 50000)
TEST_INTERVAL (V_NAME_D1 (acos), 1.0, 0x1p11, 50000)
TEST_INTERVAL (V_NAME_D1 (acos), 0x1p11, inf, 20000)
TEST_INTERVAL (V_NAME_D1 (acos), -0, -inf, 20000)
