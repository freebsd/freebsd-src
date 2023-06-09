/*
 * Double-precision SVE erf(x) function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define Scale (8.0)
#define AbsMask (0x7fffffffffffffff)

static NOINLINE sv_f64_t
__sv_erf_specialcase (sv_f64_t x, sv_f64_t y, svbool_t cmp)
{
  return sv_call_f64 (erf, x, y, cmp);
}

/* Optimized double precision SVE error function erf.
   Maximum observed error is 2.62 ULP:
   __sv_erf(0x1.79cab7e3078fap+2) got 0x1.0000000000001p+0
				 want 0x1.fffffffffffffp-1.  */
sv_f64_t
__sv_erf_x (sv_f64_t x, const svbool_t pg)
{
  /* Use top 16 bits to test for special cases and small values.  */
  sv_u64_t ix = sv_as_u64_f64 (x);
  sv_u64_t atop = svand_n_u64_x (pg, svlsr_n_u64_x (pg, ix, 48), 0x7fff);

  /* Handle both inf/nan as well as small values (|x|<2^-28).  */
  svbool_t cmp
    = svcmpge_n_u64 (pg, svsub_n_u64_x (pg, atop, 0x3e30), 0x7ff0 - 0x3e30);

  /* Get sign and absolute value.  */
  sv_f64_t a = sv_as_f64_u64 (svand_n_u64_x (pg, ix, AbsMask));
  sv_u64_t sign = svand_n_u64_x (pg, ix, ~AbsMask);

  /* i = trunc(Scale*x).  */
  sv_f64_t a_scale = svmul_n_f64_x (pg, a, Scale);
  /* Saturate index of intervals.  */
  svbool_t a_lt_6 = svcmplt_n_u64 (pg, atop, 0x4018);
  sv_u64_t i = svcvt_u64_f64_m (sv_u64 (V_ERF_NINTS - 1), a_lt_6, a_scale);

  /* Load polynomial coefficients.  */
  sv_f64_t P_0 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[0], i);
  sv_f64_t P_1 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[1], i);
  sv_f64_t P_2 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[2], i);
  sv_f64_t P_3 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[3], i);
  sv_f64_t P_4 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[4], i);
  sv_f64_t P_5 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[5], i);
  sv_f64_t P_6 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[6], i);
  sv_f64_t P_7 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[7], i);
  sv_f64_t P_8 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[8], i);
  sv_f64_t P_9 = sv_lookup_f64_x (pg, __v_erf_data.coeffs[9], i);

  /* Get shift and scale.  */
  sv_f64_t shift = sv_lookup_f64_x (pg, __v_erf_data.shifts, i);

  /* Transform polynomial variable.
     Set z = 0 in the boring domain to avoid overflow.  */
  sv_f64_t z = svmla_f64_m (a_lt_6, shift, sv_f64 (Scale), a);

  /* Evaluate polynomial P(z) using level-2 Estrin.  */
  sv_f64_t r1 = sv_fma_f64_x (pg, z, P_1, P_0);
  sv_f64_t r2 = sv_fma_f64_x (pg, z, P_3, P_2);
  sv_f64_t r3 = sv_fma_f64_x (pg, z, P_5, P_4);
  sv_f64_t r4 = sv_fma_f64_x (pg, z, P_7, P_6);
  sv_f64_t r5 = sv_fma_f64_x (pg, z, P_9, P_8);

  sv_f64_t z2 = svmul_f64_x (pg, z, z);
  sv_f64_t z4 = svmul_f64_x (pg, z2, z2);

  sv_f64_t q2 = sv_fma_f64_x (pg, r4, z2, r3);
  sv_f64_t q1 = sv_fma_f64_x (pg, r2, z2, r1);

  sv_f64_t y = sv_fma_f64_x (pg, z4, r5, q2);
  y = sv_fma_f64_x (pg, z4, y, q1);

  /* y = erf(x) if x > 0, -erf(-x) otherwise.  */
  y = sv_as_f64_u64 (sveor_u64_x (pg, sv_as_u64_f64 (y), sign));

  if (unlikely (svptest_any (pg, cmp)))
    return __sv_erf_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_erf_x, _ZGVsMxv_erf)

PL_SIG (SV, D, 1, erf, -4.0, 4.0)
PL_TEST_ULP (__sv_erf, 2.13)
PL_TEST_INTERVAL (__sv_erf, 0, 0x1p-28, 20000)
PL_TEST_INTERVAL (__sv_erf, 0x1p-28, 1, 60000)
PL_TEST_INTERVAL (__sv_erf, 1, 0x1p28, 60000)
PL_TEST_INTERVAL (__sv_erf, 0x1p28, inf, 20000)
PL_TEST_INTERVAL (__sv_erf, -0, -0x1p-28, 20000)
PL_TEST_INTERVAL (__sv_erf, -0x1p-28, -1, 60000)
PL_TEST_INTERVAL (__sv_erf, -1, -0x1p28, 60000)
PL_TEST_INTERVAL (__sv_erf, -0x1p28, -inf, 20000)
#endif
