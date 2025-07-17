/*
 * Double-precision vector asin(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float64x2_t c0, c2, c4, c6, c8, c10;
  float64x2_t pi_over_2;
  uint64x2_t abs_mask;
  double c1, c3, c5, c7, c9, c11;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  .c0 = V2 (0x1.555555555554ep-3),	  .c1 = 0x1.3333333337233p-4,
  .c2 = V2 (0x1.6db6db67f6d9fp-5),	  .c3 = 0x1.f1c71fbd29fbbp-6,
  .c4 = V2 (0x1.6e8b264d467d6p-6),	  .c5 = 0x1.1c5997c357e9dp-6,
  .c6 = V2 (0x1.c86a22cd9389dp-7),	  .c7 = 0x1.856073c22ebbep-7,
  .c8 = V2 (0x1.fd1151acb6bedp-8),	  .c9 = 0x1.087182f799c1dp-6,
  .c10 = V2 (-0x1.6602748120927p-7),	  .c11 = 0x1.cfa0dd1f9478p-6,
  .pi_over_2 = V2 (0x1.921fb54442d18p+0), .abs_mask = V2 (0x7fffffffffffffff),
};

#define AllMask v_u64 (0xffffffffffffffff)
#define One 0x3ff0000000000000
#define Small 0x3e50000000000000 /* 2^-12.  */

#if WANT_SIMD_EXCEPT
static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (asin, x, y, special);
}
#endif

/* Double-precision implementation of vector asin(x).

   For |x| < Small, approximate asin(x) by x. Small = 2^-12 for correct
   rounding. If WANT_SIMD_EXCEPT = 0, Small = 0 and we proceed with the
   following approximation.

   For |x| in [Small, 0.5], use an order 11 polynomial P such that the final
   approximation is an odd polynomial: asin(x) ~ x + x^3 P(x^2).

   The largest observed error in this region is 1.01 ulps,
   _ZGVnN2v_asin (0x1.da9735b5a9277p-2) got 0x1.ed78525a927efp-2
				       want 0x1.ed78525a927eep-2.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     asin(x) = pi/2 - (y + y * z * P(z)), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.69 ulps,
   _ZGVnN2v_asin (0x1.044e8cefee301p-1) got 0x1.1111dd54ddf96p-1
				       want 0x1.1111dd54ddf99p-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (asin) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  float64x2_t ax = vabsq_f64 (x);

#if WANT_SIMD_EXCEPT
  /* Special values need to be computed with scalar fallbacks so
     that appropriate exceptions are raised.  */
  uint64x2_t special
      = vcgtq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (ax), v_u64 (Small)),
		   v_u64 (One - Small));
  if (unlikely (v_any_u64 (special)))
    return special_case (x, x, AllMask);
#endif

  uint64x2_t a_lt_half = vcaltq_f64 (x, v_f64 (0.5));

  /* Evaluate polynomial Q(x) = y + y * z * P(z) with
     z = x ^ 2 and y = |x|            , if |x| < 0.5
     z = (1 - |x|) / 2 and y = sqrt(z), if |x| >= 0.5.  */
  float64x2_t z2 = vbslq_f64 (a_lt_half, vmulq_f64 (x, x),
			      vfmsq_n_f64 (v_f64 (0.5), ax, 0.5));
  float64x2_t z = vbslq_f64 (a_lt_half, ax, vsqrtq_f64 (z2));

  /* Use a single polynomial approximation P for both intervals.  */
  float64x2_t z4 = vmulq_f64 (z2, z2);
  float64x2_t z8 = vmulq_f64 (z4, z4);
  float64x2_t z16 = vmulq_f64 (z8, z8);

  /* order-11 estrin.  */
  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t c57 = vld1q_f64 (&d->c5);
  float64x2_t c911 = vld1q_f64 (&d->c9);

  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, z2, c13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, z2, c13, 1);
  float64x2_t p03 = vfmaq_f64 (p01, z4, p23);

  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, z2, c57, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, z2, c57, 1);
  float64x2_t p47 = vfmaq_f64 (p45, z4, p67);

  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, z2, c911, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, z2, c911, 1);
  float64x2_t p811 = vfmaq_f64 (p89, z4, p1011);

  float64x2_t p07 = vfmaq_f64 (p03, z8, p47);
  float64x2_t p = vfmaq_f64 (p07, z16, p811);

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = vfmaq_f64 (z, vmulq_f64 (z, z2), p);

  /* asin(|x|) = Q(|x|)         , for |x| < 0.5
	       = pi/2 - 2 Q(|x|), for |x| >= 0.5.  */
  float64x2_t y = vbslq_f64 (a_lt_half, p, vfmsq_n_f64 (d->pi_over_2, p, 2.0));

  /* Copy sign.  */
  return vbslq_f64 (d->abs_mask, y, x);
}

TEST_SIG (V, D, 1, asin, -1.0, 1.0)
TEST_ULP (V_NAME_D1 (asin), 2.20)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (asin), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_D1 (asin), 0, Small, 5000)
TEST_INTERVAL (V_NAME_D1 (asin), Small, 0.5, 50000)
TEST_INTERVAL (V_NAME_D1 (asin), 0.5, 1.0, 50000)
TEST_INTERVAL (V_NAME_D1 (asin), 1.0, 0x1p11, 50000)
TEST_INTERVAL (V_NAME_D1 (asin), 0x1p11, inf, 20000)
TEST_INTERVAL (V_NAME_D1 (asin), -0, -inf, 20000)
