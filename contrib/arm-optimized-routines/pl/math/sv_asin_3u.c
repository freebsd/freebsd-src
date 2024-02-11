/*
 * Double-precision SVE asin(x) function.
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
  float64_t pi_over_2f;
} data = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  .poly = { 0x1.555555555554ep-3, 0x1.3333333337233p-4,
	    0x1.6db6db67f6d9fp-5, 0x1.f1c71fbd29fbbp-6,
	    0x1.6e8b264d467d6p-6, 0x1.1c5997c357e9dp-6,
	    0x1.c86a22cd9389dp-7, 0x1.856073c22ebbep-7,
	    0x1.fd1151acb6bedp-8, 0x1.087182f799c1dp-6,
	    -0x1.6602748120927p-7, 0x1.cfa0dd1f9478p-6, },
  .pi_over_2f = 0x1.921fb54442d18p+0,
};

#define P(i) sv_f64 (d->poly[i])

/* Double-precision SVE implementation of vector asin(x).

   For |x| in [0, 0.5], use an order 11 polynomial P such that the final
   approximation is an odd polynomial: asin(x) ~ x + x^3 P(x^2).

   The largest observed error in this region is 0.52 ulps,
   _ZGVsMxv_asin(0x1.d95ae04998b6cp-2) got 0x1.ec13757305f27p-2
				      want 0x1.ec13757305f26p-2.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     asin(x) = pi/2 - (y + y * z * P(z)), with  z = (1-x)/2 and y = sqrt(z).

   The largest observed error in this region is 2.69 ulps,
   _ZGVsMxv_asin(0x1.044ac9819f573p-1) got 0x1.110d7e85fdd5p-1
				      want 0x1.110d7e85fdd53p-1.  */
svfloat64_t SV_NAME_D1 (asin) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t sign = svand_x (pg, svreinterpret_u64 (x), 0x8000000000000000);
  svfloat64_t ax = svabs_x (pg, x);
  svbool_t a_ge_half = svacge (pg, x, 0.5);

  /* Evaluate polynomial Q(x) = y + y * z * P(z) with
     z = x ^ 2 and y = |x|            , if |x| < 0.5
     z = (1 - |x|) / 2 and y = sqrt(z), if |x| >= 0.5.  */
  svfloat64_t z2 = svsel (a_ge_half, svmls_x (pg, sv_f64 (0.5), ax, 0.5),
			  svmul_x (pg, x, x));
  svfloat64_t z = svsqrt_m (ax, a_ge_half, z2);

  /* Use a single polynomial approximation P for both intervals.  */
  svfloat64_t z4 = svmul_x (pg, z2, z2);
  svfloat64_t z8 = svmul_x (pg, z4, z4);
  svfloat64_t z16 = svmul_x (pg, z8, z8);
  svfloat64_t p = sv_estrin_11_f64_x (pg, z2, z4, z8, z16, d->poly);
  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = svmla_x (pg, z, svmul_x (pg, z, z2), p);

  /* asin(|x|) = Q(|x|)         , for |x| < 0.5
	       = pi/2 - 2 Q(|x|), for |x| >= 0.5.  */
  svfloat64_t y = svmad_m (a_ge_half, p, sv_f64 (-2.0), d->pi_over_2f);

  /* Copy sign.  */
  return svreinterpret_f64 (svorr_x (pg, svreinterpret_u64 (y), sign));
}

PL_SIG (SV, D, 1, asin, -1.0, 1.0)
PL_TEST_ULP (SV_NAME_D1 (asin), 2.19)
PL_TEST_INTERVAL (SV_NAME_D1 (asin), 0, 0.5, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (asin), 0.5, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (asin), 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (asin), 0x1p11, inf, 20000)
PL_TEST_INTERVAL (SV_NAME_D1 (asin), -0, -inf, 20000)
