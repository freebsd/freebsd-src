/*
 * Single-precision vector erf(x) function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define AbsMask (0x7fffffff)

static NOINLINE sv_f32_t
__sv_erff_specialcase (sv_f32_t x, sv_f32_t y, svbool_t cmp)
{
  return sv_call_f32 (erff, x, y, cmp);
}

sv_f32_t __sv_expf_x (svbool_t, sv_f32_t);

/* Optimized single precision vector erf. Worst-case error is 1.25 ULP:
   __sv_erff(0x1.dc59fap-1) got 0x1.9f9c88p-1
			   want 0x1.9f9c8ap-1.  */
sv_f32_t
__sv_erff_x (sv_f32_t x, const svbool_t pg)
{
  sv_u32_t ix = sv_as_u32_f32 (x);
  sv_u32_t atop = svand_n_u32_x (pg, svlsr_n_u32_x (pg, ix, 16), 0x7fff);
  /* Handle both inf/nan as well as small values (|x|<2^-28).  */
  svbool_t cmp
    = svcmpge_n_u32 (pg, svsub_n_u32_x (pg, atop, 0x3180), 0x7ff0 - 0x3180);

  sv_u32_t sign = svand_n_u32_x (pg, ix, ~AbsMask);
  /* |x| < 0.921875.  */
  svbool_t red = svaclt_n_f32 (pg, x, 0.921875f);
  /* |x| > 4.0.  */
  svbool_t bor = svacgt_n_f32 (pg, x, 4.0f);

  /* Load polynomial coefficients.  */
  sv_u32_t idx_lo = svsel (red, sv_u32 (0), sv_u32 (1));
  sv_u32_t idx_hi = svadd_n_u32_x (pg, idx_lo, 2);

  const float *base = (float *) __v_erff_data.coeffs;
  sv_f32_t c_2_5 = svld1rq (svptrue_b32 (), base + 2);
  sv_f32_t c_6_9 = svld1rq (svptrue_b32 (), base + 6);
  sv_f32_t c_10_13 = svld1rq (svptrue_b32 (), base + 10);

  /* Do not need to store elem 0 of __v_erff_data as it is not used.  */
  sv_f32_t p1 = svtbl (c_2_5, idx_lo);
  sv_f32_t p2 = svtbl (c_2_5, idx_hi);
  sv_f32_t p3 = svtbl (c_6_9, idx_lo);
  sv_f32_t p4 = svtbl (c_6_9, idx_hi);
  sv_f32_t p5 = svtbl (c_10_13, idx_lo);
  sv_f32_t p6 = svtbl (c_10_13, idx_hi);

  sv_f32_t a = svabs_f32_x (pg, x);
  /* Square with merging mul - z is x^2 for reduced, |x| otherwise.  */
  sv_f32_t z = svmul_f32_m (red, a, a);

  /* Evaluate polynomial on |x| or x^2.  */
  sv_f32_t r = sv_fma_f32_x (pg, z, p6, p5);
  r = sv_fma_f32_x (pg, z, r, p4);
  r = sv_fma_f32_x (pg, z, r, p3);
  r = sv_fma_f32_x (pg, z, r, p2);
  r = sv_fma_f32_x (pg, z, r, p1);
  /* Use merging svmad for last operation - apply first coefficient if not
     reduced, otherwise r is propagated unchanged. This is because the reduced
     polynomial has lower order than the non-reduced.  */
  r = svmad_n_f32_m (svnot_b_z (pg, red), r, z, base[1]);
  r = sv_fma_f32_x (pg, a, r, a);

  /* y = |x| + |x| * P(x^2)               if |x| < 0.921875
     y = 1 - exp (-(|x| + |x| * P(|x|)))  otherwise.  */
  sv_f32_t y = __sv_expf_x (pg, svneg_f32_x (pg, r));
  y = svsel_f32 (red, r, svsubr_n_f32_x (pg, y, 1.0));

  /* Boring domain (absolute value is required to get the sign of erf(-nan)
     right).  */
  y = svsel_f32 (bor, sv_f32 (1.0f), svabs_f32_x (pg, y));

  /* y = erf(x) if x>0, -erf(-x) otherwise.  */
  y = sv_as_f32_u32 (sveor_u32_x (pg, sv_as_u32_f32 (y), sign));

  if (unlikely (svptest_any (pg, cmp)))
    return __sv_erff_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_erff_x, _ZGVsMxv_erff)

PL_SIG (SV, F, 1, erf, -4.0, 4.0)
PL_TEST_ULP (__sv_erff, 0.76)
PL_TEST_INTERVAL (__sv_erff, 0, 0x1p-28, 20000)
PL_TEST_INTERVAL (__sv_erff, 0x1p-28, 1, 60000)
PL_TEST_INTERVAL (__sv_erff, 1, 0x1p28, 60000)
PL_TEST_INTERVAL (__sv_erff, 0x1p28, inf, 20000)
PL_TEST_INTERVAL (__sv_erff, -0, -0x1p-28, 20000)
PL_TEST_INTERVAL (__sv_erff, -0x1p-28, -1, 60000)
PL_TEST_INTERVAL (__sv_erff, -1, -0x1p28, 60000)
PL_TEST_INTERVAL (__sv_erff, -0x1p28, -inf, 20000)
#endif
