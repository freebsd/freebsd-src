/*
 * Double-precision SVE cbrt(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f64.h"

const static struct data
{
  float64_t poly[4];
  float64_t table[5];
  float64_t one_third, two_thirds, shift;
  int64_t exp_bias;
  uint64_t tiny_bound, thresh;
} data = {
  /* Generated with FPMinimax in [0.5, 1].  */
  .poly = { 0x1.c14e8ee44767p-2, 0x1.dd2d3f99e4c0ep-1, -0x1.08e83026b7e74p-1,
	    0x1.2c74eaa3ba428p-3, },
  /* table[i] = 2^((i - 2) / 3).  */
  .table = { 0x1.428a2f98d728bp-1, 0x1.965fea53d6e3dp-1, 0x1p0,
	     0x1.428a2f98d728bp0, 0x1.965fea53d6e3dp0, },
  .one_third = 0x1.5555555555555p-2,
  .two_thirds = 0x1.5555555555555p-1,
  .shift = 0x1.8p52,
  .exp_bias = 1022,
  .tiny_bound = 0x0010000000000000, /* Smallest normal.  */
  .thresh = 0x7fe0000000000000, /* asuint64 (infinity) - tiny_bound.  */
};

#define MantissaMask 0x000fffffffffffff
#define HalfExp 0x3fe0000000000000

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (cbrt, x, y, special);
}

static inline svfloat64_t
shifted_lookup (const svbool_t pg, const float64_t *table, svint64_t i)
{
  return svld1_gather_index (pg, table, svadd_x (pg, i, 2));
}

/* Approximation for double-precision vector cbrt(x), using low-order
   polynomial and two Newton iterations. Greatest observed error is 1.79 ULP.
   Errors repeat according to the exponent, for instance an error observed for
   double value m * 2^e will be observed for any input m * 2^(e + 3*i), where i
   is an integer.
   _ZGVsMxv_cbrt (0x0.3fffb8d4413f3p-1022) got 0x1.965f53b0e5d97p-342
					  want 0x1.965f53b0e5d95p-342.  */
svfloat64_t SV_NAME_D1 (cbrt) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t ax = svabs_x (pg, x);
  svuint64_t iax = svreinterpret_u64 (ax);
  svuint64_t sign = sveor_x (pg, svreinterpret_u64 (x), iax);

  /* Subnormal, +/-0 and special values.  */
  svbool_t special = svcmpge (pg, svsub_x (pg, iax, d->tiny_bound), d->thresh);

  /* Decompose |x| into m * 2^e, where m is in [0.5, 1.0]. This is a vector
     version of frexp, which gets subnormal values wrong - these have to be
     special-cased as a result.  */
  svfloat64_t m = svreinterpret_f64 (svorr_x (
      pg, svand_x (pg, svreinterpret_u64 (x), MantissaMask), HalfExp));
  svint64_t e
      = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, iax, 52)), d->exp_bias);

  /* Calculate rough approximation for cbrt(m) in [0.5, 1.0], starting point
     for Newton iterations.  */
  svfloat64_t p
      = sv_pairwise_poly_3_f64_x (pg, m, svmul_x (pg, m, m), d->poly);

  /* Two iterations of Newton's method for iteratively approximating cbrt.  */
  svfloat64_t m_by_3 = svmul_x (pg, m, d->one_third);
  svfloat64_t a = svmla_x (pg, svdiv_x (pg, m_by_3, svmul_x (pg, p, p)), p,
			   d->two_thirds);
  a = svmla_x (pg, svdiv_x (pg, m_by_3, svmul_x (pg, a, a)), a, d->two_thirds);

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     We can get 2 ^ round(e / 3) using ldexp and integer divide, but since e is
     not necessarily a multiple of 3 we lose some information.

     Let q = 2 ^ round(e / 3), then t = 2 ^ (e / 3) / q.

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3, which
     is an integer in [-2, 2], and can be looked up in the table T. Hence the
     result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.  */
  svfloat64_t eb3f = svmul_x (pg, svcvt_f64_x (pg, e), d->one_third);
  svint64_t ey = svcvt_s64_x (pg, eb3f);
  svint64_t em3 = svmls_x (pg, e, ey, 3);

  svfloat64_t my = shifted_lookup (pg, d->table, em3);
  my = svmul_x (pg, my, a);

  /* Vector version of ldexp.  */
  svfloat64_t y = svscale_x (pg, my, ey);

  if (unlikely (svptest_any (pg, special)))
    return special_case (
	x, svreinterpret_f64 (svorr_x (pg, svreinterpret_u64 (y), sign)),
	special);

  /* Copy sign.  */
  return svreinterpret_f64 (svorr_x (pg, svreinterpret_u64 (y), sign));
}

PL_SIG (SV, D, 1, cbrt, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_D1 (cbrt), 1.30)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (cbrt), 0, inf, 1000000)
