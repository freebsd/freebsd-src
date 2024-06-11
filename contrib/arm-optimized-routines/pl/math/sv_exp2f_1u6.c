/*
 * Single-precision SVE 2^x function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f32.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float poly[5];
  float shift, thres;
} data = {
  /* Coefficients copied from the polynomial in AdvSIMD variant, reversed for
     compatibility with polynomial helpers.  */
  .poly = { 0x1.62e422p-1f, 0x1.ebf9bcp-3f, 0x1.c6bd32p-5f, 0x1.3ce9e4p-7f,
	    0x1.59977ap-10f },
  /* 1.5*2^17 + 127.  */
  .shift = 0x1.903f8p17f,
  /* Roughly 87.3. For x < -Thres, the result is subnormal and not handled
     correctly by FEXPA.  */
  .thres = 0x1.5d5e2ap+6f,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (exp2f, x, y, special);
}

/* Single-precision SVE exp2f routine. Implements the same algorithm
   as AdvSIMD exp2f.
   Worst case error is 1.04 ULPs.
   SV_NAME_F1 (exp2)(0x1.943b9p-1) got 0x1.ba7eb2p+0
				  want 0x1.ba7ebp+0.  */
svfloat32_t SV_NAME_F1 (exp2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
    x = n + r, with r in [-1/2, 1/2].  */
  svfloat32_t shift = sv_f32 (d->shift);
  svfloat32_t z = svadd_x (pg, x, shift);
  svfloat32_t n = svsub_x (pg, z, shift);
  svfloat32_t r = svsub_x (pg, x, n);

  svbool_t special = svacgt (pg, x, d->thres);
  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* Polynomial evaluation: poly(r) ~ exp2(r)-1.
     Evaluate polynomial use hybrid scheme - offset ESTRIN by 1 for
     coefficients 1 to 4, and apply most significant coefficient directly.  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t p14 = sv_pairwise_poly_3_f32_x (pg, r, r2, d->poly + 1);
  svfloat32_t p0 = svmul_x (pg, r, d->poly[0]);
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svmla_x (pg, scale, scale, poly), special);

  return svmla_x (pg, scale, scale, poly);
}

PL_SIG (SV, F, 1, exp2, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_F1 (exp2), 0.55)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), 0, Thres, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), Thres, 1, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), 1, Thres, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), Thres, inf, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), -0, -0x1p-23, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), -0x1p-23, -1, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), -1, -0x1p23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), -0x1p23, -inf, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), -0, ScaleThres, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), ScaleThres, -1, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), -1, ScaleThres, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp2), ScaleThres, -inf, 50000)
