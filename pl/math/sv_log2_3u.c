/*
 * Double-precision SVE log2 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f64.h"

#define N (1 << V_LOG2_TABLE_BITS)
#define Off 0x3fe6900900000000
#define Max (0x7ff0000000000000)
#define Min (0x0010000000000000)
#define Thresh (0x7fe0000000000000) /* Max - Min.  */

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t cmp)
{
  return sv_call_f64 (log2, x, y, cmp);
}

/* Double-precision SVE log2 routine.
   Implements the same algorithm as AdvSIMD log10, with coefficients and table
   entries scaled in extended precision.
   The maximum observed error is 2.58 ULP:
   SV_NAME_D1 (log2)(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
					  want 0x1.fffb34198d9ddp-5.  */
svfloat64_t SV_NAME_D1 (log2) (svfloat64_t x, const svbool_t pg)
{
  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, Min), Thresh);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, Off);
  svuint64_t i = svlsr_x (pg, tmp, 51 - V_LOG2_TABLE_BITS);
  i = svand_x (pg, i, (N - 1) << 1);
  svfloat64_t k = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  svfloat64_t z = svreinterpret_f64 (
      svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52)));

  svfloat64_t invc = svld1_gather_index (pg, &__v_log2_data.table[0].invc, i);
  svfloat64_t log2c
      = svld1_gather_index (pg, &__v_log2_data.table[0].log2c, i);

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */

  svfloat64_t r = svmad_x (pg, invc, z, -1.0);
  svfloat64_t w = svmla_x (pg, log2c, r, __v_log2_data.invln2);

  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t y = sv_pw_horner_4_f64_x (pg, r, r2, __v_log2_data.poly);
  w = svadd_x (pg, k, w);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svmla_x (svnot_z (pg, special), w, r2, y),
			 special);
  return svmla_x (pg, w, r2, y);
}

PL_SIG (SV, D, 1, log2, 0.01, 11.1)
PL_TEST_ULP (SV_NAME_D1 (log2), 2.09)
PL_TEST_EXPECT_FENV_ALWAYS (SV_NAME_D1 (log2))
PL_TEST_INTERVAL (SV_NAME_D1 (log2), -0.0, -0x1p126, 1000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 0.0, 0x1p-126, 4000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 1.0, 100, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 100, inf, 50000)
