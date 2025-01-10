/*
 * Single-precision vector log function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float poly_0135[4];
  float poly_246[3];
  float ln2;
  uint32_t off, lower;
} data = {
  .poly_0135 = {
    /* Coefficients copied from the AdvSIMD routine in math/, then rearranged so
       that coeffs 0, 1, 3 and 5 can be loaded as a single quad-word, hence used
       with _lane variant of MLA intrinsic.  */
    -0x1.3e737cp-3f, 0x1.5a9aa2p-3f, 0x1.961348p-3f, 0x1.555d7cp-2f
  },
  .poly_246 = { -0x1.4f9934p-3f, -0x1.00187cp-2f, -0x1.ffffc8p-2f },
  .ln2 = 0x1.62e43p-1f,
  .off = 0x3f2aaaab,
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .lower = 0x00800000 - 0x3f2aaaab
};

#define Thresh (0x7f000000) /* asuint32(inf) - 0x00800000.  */
#define Mask (0x007fffff)

static svfloat32_t NOINLINE
special_case (svuint32_t u_off, svfloat32_t p, svfloat32_t r2, svfloat32_t y,
	      svbool_t cmp)
{
  return sv_call_f32 (
      logf, svreinterpret_f32 (svadd_x (svptrue_b32 (), u_off, data.off)),
      svmla_x (svptrue_b32 (), p, r2, y), cmp);
}

/* Optimised implementation of SVE logf, using the same algorithm and
   polynomial as the AdvSIMD routine. Maximum error is 3.34 ULPs:
   SV_NAME_F1 (log)(0x1.557298p+0) got 0x1.26edecp-2
				  want 0x1.26ede6p-2.  */
svfloat32_t SV_NAME_F1 (log) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u_off = svreinterpret_u32 (x);

  u_off = svsub_x (pg, u_off, d->off);
  svbool_t cmp = svcmpge (pg, svsub_x (pg, u_off, d->lower), Thresh);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u_off), 23)); /* Sign-extend.  */

  svuint32_t u = svand_x (pg, u_off, Mask);
  u = svadd_x (pg, u, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* y = log(1+r) + n*ln2.  */
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
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
    return special_case (u_off, p, r2, y, cmp);
  return svmla_x (pg, p, r2, y);
}

TEST_SIG (SV, F, 1, log, 0.01, 11.1)
TEST_ULP (SV_NAME_F1 (log), 2.85)
TEST_DISABLE_FENV (SV_NAME_F1 (log))
TEST_INTERVAL (SV_NAME_F1 (log), -0.0, -inf, 100)
TEST_INTERVAL (SV_NAME_F1 (log), 0, 0x1p-126, 100)
TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (SV_NAME_F1 (log), 1.0, 100, 50000)
TEST_INTERVAL (SV_NAME_F1 (log), 100, inf, 50000)
CLOSE_SVE_ATTR
