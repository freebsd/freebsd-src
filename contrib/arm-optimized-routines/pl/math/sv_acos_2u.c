/*
 * Double-precision SVE acos(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float64_t poly[12];
  float64_t pi, pi_over_2;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  .poly = { 0x1.555555555554ep-3, 0x1.3333333337233p-4, 0x1.6db6db67f6d9fp-5,
	    0x1.f1c71fbd29fbbp-6, 0x1.6e8b264d467d6p-6, 0x1.1c5997c357e9dp-6,
	    0x1.c86a22cd9389dp-7, 0x1.856073c22ebbep-7, 0x1.fd1151acb6bedp-8,
	    0x1.087182f799c1dp-6, -0x1.6602748120927p-7, 0x1.cfa0dd1f9478p-6, },
  .pi = 0x1.921fb54442d18p+1,
  .pi_over_2 = 0x1.921fb54442d18p+0,
};

/* Double-precision SVE implementation of vector acos(x).

   For |x| in [0, 0.5], use an order 11 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

   The largest observed error in this region is 1.18 ulps,
   _ZGVsMxv_acos (0x1.fbc5fe28ee9e3p-2) got 0x1.0d4d0f55667f6p+0
				       want 0x1.0d4d0f55667f7p+0.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 1.52 ulps,
   _ZGVsMxv_acos (0x1.24024271a500ap-1) got 0x1.ed82df4243f0dp-1
				       want 0x1.ed82df4243f0bp-1.  */
svfloat64_t SV_NAME_D1 (acos) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t sign = svand_x (pg, svreinterpret_u64 (x), 0x8000000000000000);
  svfloat64_t ax = svabs_x (pg, x);

  svbool_t a_gt_half = svacgt (pg, x, 0.5);

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  svfloat64_t z2 = svsel (a_gt_half, svmls_x (pg, sv_f64 (0.5), ax, 0.5),
			  svmul_x (pg, x, x));
  svfloat64_t z = svsqrt_m (ax, a_gt_half, z2);

  /* Use a single polynomial approximation P for both intervals.  */
  svfloat64_t z4 = svmul_x (pg, z2, z2);
  svfloat64_t z8 = svmul_x (pg, z4, z4);
  svfloat64_t z16 = svmul_x (pg, z8, z8);
  svfloat64_t p = sv_estrin_11_f64_x (pg, z2, z4, z8, z16, d->poly);

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = svmla_x (pg, z, svmul_x (pg, z, z2), p);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  svfloat64_t y
      = svreinterpret_f64 (svorr_x (pg, svreinterpret_u64 (p), sign));

  svbool_t is_neg = svcmplt (pg, x, 0.0);
  svfloat64_t off = svdup_f64_z (is_neg, d->pi);
  svfloat64_t mul = svsel (a_gt_half, sv_f64 (2.0), sv_f64 (-1.0));
  svfloat64_t add = svsel (a_gt_half, off, sv_f64 (d->pi_over_2));

  return svmla_x (pg, add, mul, y);
}

PL_SIG (SV, D, 1, acos, -1.0, 1.0)
PL_TEST_ULP (SV_NAME_D1 (acos), 1.02)
PL_TEST_INTERVAL (SV_NAME_D1 (acos), 0, 0.5, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (acos), 0.5, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (acos), 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (acos), 0x1p11, inf, 20000)
PL_TEST_INTERVAL (SV_NAME_D1 (acos), -0, -inf, 20000)
