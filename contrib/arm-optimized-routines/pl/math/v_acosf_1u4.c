/*
 * Single-precision vector acos(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f32.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float32x4_t poly[5];
  float32x4_t pi_over_2f, pif;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))  on
     [ 0x1p-24 0x1p-2 ] order = 4 rel error: 0x1.00a23bbp-29 .  */
  .poly = { V4 (0x1.55555ep-3), V4 (0x1.33261ap-4), V4 (0x1.70d7dcp-5),
	    V4 (0x1.b059dp-6), V4 (0x1.3af7d8p-5) },
  .pi_over_2f = V4 (0x1.921fb6p+0f),
  .pif = V4 (0x1.921fb6p+1f),
};

#define AbsMask 0x7fffffff
#define Half 0x3f000000
#define One 0x3f800000
#define Small 0x32800000 /* 2^-26.  */

#if WANT_SIMD_EXCEPT
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t special)
{
  return v_call_f32 (acosf, x, y, special);
}
#endif

/* Single-precision implementation of vector acos(x).

   For |x| < Small, approximate acos(x) by pi/2 - x. Small = 2^-26 for correct
   rounding.
   If WANT_SIMD_EXCEPT = 0, Small = 0 and we proceed with the following
   approximation.

   For |x| in [Small, 0.5], use order 4 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

    The largest observed error in this region is 1.26 ulps,
      _ZGVnN4v_acosf (0x1.843bfcp-2) got 0x1.2e934cp+0 want 0x1.2e934ap+0.

    For |x| in [0.5, 1.0], use same approximation with a change of variable

      acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 1.32 ulps,
   _ZGVnN4v_acosf (0x1.15ba56p-1) got 0x1.feb33p-1
			   want 0x1.feb32ep-1.  */
float32x4_t VPCS_ATTR V_NAME_F1 (acos) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t ia = vandq_u32 (ix, v_u32 (AbsMask));

#if WANT_SIMD_EXCEPT
  /* A single comparison for One, Small and QNaN.  */
  uint32x4_t special
      = vcgtq_u32 (vsubq_u32 (ia, v_u32 (Small)), v_u32 (One - Small));
  if (unlikely (v_any_u32 (special)))
    return special_case (x, x, v_u32 (0xffffffff));
#endif

  float32x4_t ax = vreinterpretq_f32_u32 (ia);
  uint32x4_t a_le_half = vcleq_u32 (ia, v_u32 (Half));

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float32x4_t z2 = vbslq_f32 (a_le_half, vmulq_f32 (x, x),
			      vfmsq_n_f32 (v_f32 (0.5), ax, 0.5));
  float32x4_t z = vbslq_f32 (a_le_half, ax, vsqrtq_f32 (z2));

  /* Use a single polynomial approximation P for both intervals.  */
  float32x4_t p = v_horner_4_f32 (z2, d->poly);
  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = vfmaq_f32 (z, vmulq_f32 (z, z2), p);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  float32x4_t y = vbslq_f32 (v_u32 (AbsMask), p, x);

  uint32x4_t is_neg = vcltzq_f32 (x);
  float32x4_t off = vreinterpretq_f32_u32 (
      vandq_u32 (vreinterpretq_u32_f32 (d->pif), is_neg));
  float32x4_t mul = vbslq_f32 (a_le_half, v_f32 (-1.0), v_f32 (2.0));
  float32x4_t add = vbslq_f32 (a_le_half, d->pi_over_2f, off);

  return vfmaq_f32 (add, mul, y);
}

PL_SIG (V, F, 1, acos, -1.0, 1.0)
PL_TEST_ULP (V_NAME_F1 (acos), 0.82)
PL_TEST_EXPECT_FENV (V_NAME_F1 (acos), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_F1 (acos), 0, 0x1p-26, 5000)
PL_TEST_INTERVAL (V_NAME_F1 (acos), 0x1p-26, 0.5, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (acos), 0.5, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (acos), 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (acos), 0x1p11, inf, 20000)
PL_TEST_INTERVAL (V_NAME_F1 (acos), -0, -inf, 20000)
