/*
 * Single-precision vector log function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float poly_0135[4];
  float poly_246[3];
  float ln2;
} data = {
  .poly_0135 = {
    /* Coefficients copied from the AdvSIMD routine in math/, then rearranged so
       that coeffs 0, 1, 3 and 5 can be loaded as a single quad-word, hence used
       with _lane variant of MLA intrinsic.  */
    -0x1.3e737cp-3f, 0x1.5a9aa2p-3f, 0x1.961348p-3f, 0x1.555d7cp-2f
  },
  .poly_246 = { -0x1.4f9934p-3f, -0x1.00187cp-2f, -0x1.ffffc8p-2f },
  .ln2 = 0x1.62e43p-1f
};

#define Min (0x00800000)
#define Max (0x7f800000)
#define Thresh (0x7f000000) /* Max - Min.  */
#define Mask (0x007fffff)
#define Off (0x3f2aaaab) /* 0.666667.  */

float optr_aor_log_f32 (float);

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  return sv_call_f32 (optr_aor_log_f32, x, y, cmp);
}

/* Optimised implementation of SVE logf, using the same algorithm and
   polynomial as the AdvSIMD routine. Maximum error is 3.34 ULPs:
   SV_NAME_F1 (log)(0x1.557298p+0) got 0x1.26edecp-2
				  want 0x1.26ede6p-2.  */
svfloat32_t SV_NAME_F1 (log) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u = svreinterpret_u32 (x);
  svbool_t cmp = svcmpge (pg, svsub_x (pg, u, Min), Thresh);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = svsub_x (pg, u, Off);
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u), 23)); /* Sign-extend.  */
  u = svand_x (pg, u, Mask);
  u = svadd_x (pg, u, Off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* y = log(1+r) + n*ln2.  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  /* n*ln2 + r + r2*(P6 + r*P5 + r2*(P4 + r*P3 + r2*(P2 + r*P1 + r2*P0))).  */
  svfloat32_t p_0135 = svld1rq (svptrue_b32 (), &d->poly_0135[0]);
  svfloat32_t p = svmla_lane (sv_f32 (d->poly_246[0]), r, p_0135, 1);
  svfloat32_t q = svmla_lane (sv_f32 (d->poly_246[1]), r, p_0135, 2);
  svfloat32_t y = svmla_lane (sv_f32 (d->poly_246[2]), r, p_0135, 3);
  p = svmla_lane (p, r2, p_0135, 0);

  q = svmla_x (pg, q, r2, p);
  y = svmla_x (pg, y, r2, q);
  p = svmla_x (pg, r, n, d->ln2);

  if (unlikely (svptest_any (pg, cmp)))
    return special_case (x, svmla_x (svnot_z (pg, cmp), p, r2, y), cmp);
  return svmla_x (pg, p, r2, y);
}

PL_SIG (SV, F, 1, log, 0.01, 11.1)
PL_TEST_ULP (SV_NAME_F1 (log), 2.85)
PL_TEST_INTERVAL (SV_NAME_F1 (log), -0.0, -inf, 100)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 0, 0x1p-126, 100)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 1.0, 100, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 100, inf, 50000)
