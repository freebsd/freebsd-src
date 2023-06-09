/*
 * Double-precision SVE erfc(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED
#include "sv_exp_tail.h"

sv_f64_t __sv_exp_x (sv_f64_t, svbool_t);

static NOINLINE sv_f64_t
specialcase (sv_f64_t x, sv_f64_t y, svbool_t special)
{
  return sv_call_f64 (erfc, x, y, special);
}

static inline sv_u64_t
lookup_interval_idx (const svbool_t pg, sv_f64_t abs_x)
{
  /* Interval index is calculated by (((abs(x) + 1)^4) >> 53) - 1023, bounded by
     the number of polynomials.  */
  sv_f64_t xp1 = svadd_n_f64_x (pg, abs_x, 1);
  xp1 = svmul_f64_x (pg, xp1, xp1);
  xp1 = svmul_f64_x (pg, xp1, xp1);
  sv_u64_t interval_idx
    = svsub_n_u64_x (pg, svlsr_n_u64_x (pg, sv_as_u64_f64 (xp1), 52), 1023);
  return svsel_u64 (svcmple_n_u64 (pg, interval_idx, ERFC_NUM_INTERVALS),
		    interval_idx, sv_u64 (ERFC_NUM_INTERVALS));
}

static inline sv_f64_t
sv_eval_poly (const svbool_t pg, sv_f64_t z, sv_u64_t idx)
{
  sv_u64_t offset = svmul_n_u64_x (pg, idx, ERFC_POLY_ORDER + 1);
  const double *base = &__v_erfc_data.poly[0][12];
  sv_f64_t r = sv_lookup_f64_x (pg, base, offset);
  for (int i = 0; i < ERFC_POLY_ORDER; i++)
    {
      base--;
      sv_f64_t c = sv_lookup_f64_x (pg, base, offset);
      r = sv_fma_f64_x (pg, z, r, c);
    }
  return r;
}

static inline sv_f64_t
sv_eval_gauss (const svbool_t pg, sv_f64_t abs_x)
{
  /* Accurate evaluation of exp(-x^2). This operation is sensitive to rounding
     errors in x^2, so we compute an estimate for the error and use a custom exp
     helper which corrects for the calculated error estimate.  */
  sv_f64_t a2 = svmul_f64_x (pg, abs_x, abs_x);

  /* Split abs_x into (a_hi + a_lo), where a_hi is the 'large' component and
     a_lo is the 'small' component.  */
  const sv_f64_t scale = sv_f64 (0x1.0000002p27);
  sv_f64_t a_hi = svneg_f64_x (pg, sv_fma_f64_x (pg, scale, abs_x,
						 svneg_f64_x (pg, abs_x)));
  a_hi = sv_fma_f64_x (pg, scale, abs_x, a_hi);
  sv_f64_t a_lo = svsub_f64_x (pg, abs_x, a_hi);

  sv_f64_t a_hi_neg = svneg_f64_x (pg, a_hi);
  sv_f64_t a_lo_neg = svneg_f64_x (pg, a_lo);

  /* We can then estimate the error in abs_x^2 by computing (abs_x * abs_x) -
     (a_hi + a_lo) * (a_hi + a_lo).  */
  sv_f64_t e2 = sv_fma_f64_x (pg, a_hi_neg, a_hi, a2);
  e2 = sv_fma_f64_x (pg, a_hi_neg, a_lo, e2);
  e2 = sv_fma_f64_x (pg, a_lo_neg, a_hi, e2);
  e2 = sv_fma_f64_x (pg, a_lo_neg, a_lo, e2);

  return sv_exp_tail (pg, svneg_f64_x (pg, a2), e2);
}

/* Optimized double precision vector complementary error function erfc.
   Maximum measured error is 3.64 ULP:
   __sv_erfc(0x1.4792573ee6cc7p+2) got 0x1.ff3f4c8e200d5p-42
				  want 0x1.ff3f4c8e200d9p-42.  */
sv_f64_t
__sv_erfc_x (sv_f64_t x, const svbool_t pg)
{
  sv_u64_t ix = sv_as_u64_f64 (x);
  sv_f64_t abs_x = svabs_f64_x (pg, x);
  sv_u64_t atop = svlsr_n_u64_x (pg, sv_as_u64_f64 (abs_x), 52);

  /* Outside of the 'interesting' bounds, [-6, 28], +ve goes to 0, -ve goes
     to 2. As long as the polynomial is 0 in the boring zone, we can assemble
     the result correctly. This is dealt with in two ways:

     The 'coarse approach' is that the approximation algorithm is
     zero-predicated on in_bounds = |x| < 32, which saves the need to do
     coefficient lookup etc for |x| >= 32.

     The coarse approach misses [-32, -6] and [28, 32], which are dealt with in
     the polynomial and index calculation, such that the polynomial evaluates to
     0 in these regions.  */
  /* in_bounds is true for lanes where |x| < 32.  */
  svbool_t in_bounds = svcmplt_n_u64 (pg, atop, 0x404);
  /* boring_zone = 2 for x < 0, 0 otherwise.  */
  sv_f64_t boring_zone
    = sv_as_f64_u64 (svlsl_n_u64_x (pg, svlsr_n_u64_x (pg, ix, 63), 62));
  /* Very small, nan and inf.  */
  svbool_t special_cases
    = svcmpge_n_u64 (pg, svsub_n_u64_x (pg, atop, 0x3cd), 0x432);

  /* erfc(|x|) ~= P_i(|x|-x_i)*exp(-x^2)

     Where P_i is a polynomial and x_i is an offset, both defined in
     v_erfc_data.c. i is chosen based on which interval x falls in.  */
  sv_u64_t i = lookup_interval_idx (in_bounds, abs_x);
  sv_f64_t x_i = sv_lookup_f64_x (in_bounds, __v_erfc_data.interval_bounds, i);
  sv_f64_t p = sv_eval_poly (in_bounds, svsub_f64_x (pg, abs_x, x_i), i);
  /* 'copy' sign of x to p, i.e. negate p if x is negative.  */
  sv_u64_t sign = svbic_n_u64_z (in_bounds, ix, 0x7fffffffffffffff);
  p = sv_as_f64_u64 (sveor_u64_z (in_bounds, sv_as_u64_f64 (p), sign));

  sv_f64_t e = sv_eval_gauss (in_bounds, abs_x);

  /* Assemble result: 2-p*e if x<0, p*e otherwise. No need to conditionally
     select boring_zone because P[V_ERFC_NINTS-1]=0.  */
  sv_f64_t y = sv_fma_f64_x (pg, p, e, boring_zone);

  if (unlikely (svptest_any (pg, special_cases)))
    {
      return specialcase (x, y, special_cases);
    }
  return y;
}

PL_ALIAS (__sv_erfc_x, _ZGVsMxv_erfc)

PL_SIG (SV, D, 1, erfc, -4.0, 10.0)
PL_TEST_ULP (__sv_erfc, 3.15)
PL_TEST_INTERVAL (__sv_erfc, 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (__sv_erfc, 0x1p-127, 0x1p-26, 40000)
PL_TEST_INTERVAL (__sv_erfc, -0x1p-127, -0x1p-26, 40000)
PL_TEST_INTERVAL (__sv_erfc, 0x1p-26, 0x1p5, 40000)
PL_TEST_INTERVAL (__sv_erfc, -0x1p-26, -0x1p3, 40000)
PL_TEST_INTERVAL (__sv_erfc, 0, inf, 40000)
#endif
