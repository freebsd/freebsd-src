/*
 * Single-precision vector/SVE log2 function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float poly_02468[5];
  float poly_1357[4];
  uint32_t off, lower;
} data = {
  .poly_1357 = {
    /* Coefficients copied from the AdvSIMD routine, then rearranged so that coeffs
       1, 3, 5 and 7 can be loaded as a single quad-word, hence used with _lane
       variant of MLA intrinsic.  */
    -0x1.715458p-1f, -0x1.7171a4p-2f, -0x1.e5143ep-3f, -0x1.c675bp-3f
  },
  .poly_02468 = { 0x1.715476p0f, 0x1.ec701cp-2f, 0x1.27a0b8p-2f,
		  0x1.9d8ecap-3f, 0x1.9e495p-3f },
  .off = 0x3f2aaaab,
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .lower = 0x00800000 - 0x3f2aaaab
};

#define Thresh (0x7f000000) /* asuint32(inf) - 0x00800000.  */
#define MantissaMask (0x007fffff)

static svfloat32_t NOINLINE
special_case (svuint32_t u_off, svfloat32_t p, svfloat32_t r2, svfloat32_t y,
	      svbool_t cmp)
{
  return sv_call_f32 (
      log2f, svreinterpret_f32 (svadd_x (svptrue_b32 (), u_off, data.off)),
      svmla_x (svptrue_b32 (), p, r2, y), cmp);
}

/* Optimised implementation of SVE log2f, using the same algorithm
   and polynomial as AdvSIMD log2f.
   Maximum error is 2.48 ULPs:
   SV_NAME_F1 (log2)(0x1.558174p+0) got 0x1.a9be84p-2
				   want 0x1.a9be8p-2.  */
svfloat32_t SV_NAME_F1 (log2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u_off = svreinterpret_u32 (x);

  u_off = svsub_x (pg, u_off, d->off);
  svbool_t special = svcmpge (pg, svsub_x (pg, u_off, d->lower), Thresh);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u_off), 23)); /* Sign-extend.  */
  svuint32_t u = svand_x (pg, u_off, MantissaMask);
  u = svadd_x (pg, u, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* y = log2(1+r) + n.  */
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);

  /* Evaluate polynomial using pairwise Horner scheme.  */
  svfloat32_t p_1357 = svld1rq (svptrue_b32 (), &d->poly_1357[0]);
  svfloat32_t q_01 = svmla_lane (sv_f32 (d->poly_02468[0]), r, p_1357, 0);
  svfloat32_t q_23 = svmla_lane (sv_f32 (d->poly_02468[1]), r, p_1357, 1);
  svfloat32_t q_45 = svmla_lane (sv_f32 (d->poly_02468[2]), r, p_1357, 2);
  svfloat32_t q_67 = svmla_lane (sv_f32 (d->poly_02468[3]), r, p_1357, 3);
  svfloat32_t y = svmla_x (pg, q_67, r2, sv_f32 (d->poly_02468[4]));
  y = svmla_x (pg, q_45, r2, y);
  y = svmla_x (pg, q_23, r2, y);
  y = svmla_x (pg, q_01, r2, y);

  if (unlikely (svptest_any (pg, special)))
    return special_case (u_off, n, r, y, special);
  return svmla_x (svptrue_b32 (), n, r, y);
}

TEST_SIG (SV, F, 1, log2, 0.01, 11.1)
TEST_ULP (SV_NAME_F1 (log2), 1.99)
TEST_DISABLE_FENV (SV_NAME_F1 (log2))
TEST_INTERVAL (SV_NAME_F1 (log2), -0.0, -0x1p126, 4000)
TEST_INTERVAL (SV_NAME_F1 (log2), 0.0, 0x1p-126, 4000)
TEST_INTERVAL (SV_NAME_F1 (log2), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_F1 (log2), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (SV_NAME_F1 (log2), 1.0, 100, 50000)
TEST_INTERVAL (SV_NAME_F1 (log2), 100, inf, 50000)
CLOSE_SVE_ATTR
