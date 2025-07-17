/*
 * Single-precision SVE acos(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "sv_poly_f32.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float32_t poly[5];
  float32_t pi, pi_over_2;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))  on
     [ 0x1p-24 0x1p-2 ] order = 4 rel error: 0x1.00a23bbp-29 .  */
  .poly = { 0x1.55555ep-3, 0x1.33261ap-4, 0x1.70d7dcp-5, 0x1.b059dp-6,
	    0x1.3af7d8p-5, },
  .pi = 0x1.921fb6p+1f,
  .pi_over_2 = 0x1.921fb6p+0f,
};

/* Single-precision SVE implementation of vector acos(x).

   For |x| in [0, 0.5], use order 4 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

    The largest observed error in this region is 1.16 ulps,
      _ZGVsMxv_acosf(0x1.ffbeccp-2) got 0x1.0c27f8p+0
				   want 0x1.0c27f6p+0.

    For |x| in [0.5, 1.0], use same approximation with a change of variable

      acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 1.32 ulps,
   _ZGVsMxv_acosf (0x1.15ba56p-1) got 0x1.feb33p-1
				 want 0x1.feb32ep-1.  */
svfloat32_t SV_NAME_F1 (acos) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t sign = svand_x (pg, svreinterpret_u32 (x), 0x80000000);
  svfloat32_t ax = svabs_x (pg, x);
  svbool_t a_gt_half = svacgt (pg, x, 0.5);

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  svfloat32_t z2 = svsel (a_gt_half, svmls_x (pg, sv_f32 (0.5), ax, 0.5),
			  svmul_x (pg, x, x));
  svfloat32_t z = svsqrt_m (ax, a_gt_half, z2);

  /* Use a single polynomial approximation P for both intervals.  */
  svfloat32_t p = sv_horner_4_f32_x (pg, z2, d->poly);
  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = svmla_x (pg, z, svmul_x (pg, z, z2), p);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  svfloat32_t y
      = svreinterpret_f32 (svorr_x (pg, svreinterpret_u32 (p), sign));

  svbool_t is_neg = svcmplt (pg, x, 0.0);
  svfloat32_t off = svdup_f32_z (is_neg, d->pi);
  svfloat32_t mul = svsel (a_gt_half, sv_f32 (2.0), sv_f32 (-1.0));
  svfloat32_t add = svsel (a_gt_half, off, sv_f32 (d->pi_over_2));

  return svmla_x (pg, add, mul, y);
}

TEST_SIG (SV, F, 1, acos, -1.0, 1.0)
TEST_ULP (SV_NAME_F1 (acos), 0.82)
TEST_DISABLE_FENV (SV_NAME_F1 (acos))
TEST_INTERVAL (SV_NAME_F1 (acos), 0, 0.5, 50000)
TEST_INTERVAL (SV_NAME_F1 (acos), 0.5, 1.0, 50000)
TEST_INTERVAL (SV_NAME_F1 (acos), 1.0, 0x1p11, 50000)
TEST_INTERVAL (SV_NAME_F1 (acos), 0x1p11, inf, 20000)
TEST_INTERVAL (SV_NAME_F1 (acos), -0, -inf, 20000)
CLOSE_SVE_ATTR
