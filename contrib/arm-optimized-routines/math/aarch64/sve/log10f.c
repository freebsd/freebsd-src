/*
 * Single-precision SVE log10 function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float poly_0246[4];
  float poly_1357[4];
  float ln2, inv_ln10;
  uint32_t off, lower;
} data = {
  .poly_1357 = {
    /* Coefficients copied from the AdvSIMD routine, then rearranged so that coeffs
       1, 3, 5 and 7 can be loaded as a single quad-word, hence used with _lane
       variant of MLA intrinsic.  */
    0x1.2879c8p-3f, 0x1.6408f8p-4f, 0x1.f0e514p-5f, 0x1.f5f76ap-5f
  },
  .poly_0246 = { -0x1.bcb79cp-3f, -0x1.bcd472p-4f, -0x1.246f8p-4f,
		 -0x1.0fc92cp-4f },
  .ln2 = 0x1.62e43p-1f,
  .inv_ln10 = 0x1.bcb7b2p-2f,
  .off = 0x3f2aaaab,
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .lower = 0x00800000 - 0x3f2aaaab
};

#define Thres 0x7f000000 /* asuint32(inf) - 0x00800000.  */
#define MantissaMask 0x007fffff

static svfloat32_t NOINLINE
special_case (svuint32_t u_off, svfloat32_t p, svfloat32_t r2, svfloat32_t y,
	      svbool_t cmp)
{
  return sv_call_f32 (
      log10f, svreinterpret_f32 (svadd_x (svptrue_b32 (), u_off, data.off)),
      svmla_x (svptrue_b32 (), p, r2, y), cmp);
}

/* Optimised implementation of SVE log10f using the same algorithm and
   polynomial as AdvSIMD log10f.
   Maximum error is 3.31ulps:
   SV_NAME_F1 (log10)(0x1.555c16p+0) got 0x1.ffe2fap-4
				    want 0x1.ffe2f4p-4.  */
svfloat32_t SV_NAME_F1 (log10) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u_off = svreinterpret_u32 (x);

  u_off = svsub_x (pg, u_off, d->off);
  svbool_t special = svcmpge (pg, svsub_x (pg, u_off, d->lower), Thres);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u_off), 23)); /* signextend.  */
  svuint32_t ix = svand_x (pg, u_off, MantissaMask);
  ix = svadd_x (pg, ix, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (ix), 1.0f);

  /* y = log10(1+r) + n*log10(2)
     log10(1+r) ~ r * InvLn(10) + P(r)
     where P(r) is a polynomial. Use order 9 for log10(1+x), i.e. order 8 for
     log10(1+x)/x, with x in [-1/3, 1/3] (offset=2/3).  */
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t r4 = svmul_x (svptrue_b32 (), r2, r2);
  svfloat32_t p_1357 = svld1rq (svptrue_b32 (), &d->poly_1357[0]);
  svfloat32_t q_01 = svmla_lane (sv_f32 (d->poly_0246[0]), r, p_1357, 0);
  svfloat32_t q_23 = svmla_lane (sv_f32 (d->poly_0246[1]), r, p_1357, 1);
  svfloat32_t q_45 = svmla_lane (sv_f32 (d->poly_0246[2]), r, p_1357, 2);
  svfloat32_t q_67 = svmla_lane (sv_f32 (d->poly_0246[3]), r, p_1357, 3);
  svfloat32_t q_47 = svmla_x (pg, q_45, r2, q_67);
  svfloat32_t q_03 = svmla_x (pg, q_01, r2, q_23);
  svfloat32_t y = svmla_x (pg, q_03, r4, q_47);

  /* Using hi = Log10(2)*n + r*InvLn(10) is faster but less accurate.  */
  svfloat32_t hi = svmla_x (pg, r, n, d->ln2);
  hi = svmul_x (pg, hi, d->inv_ln10);

  if (unlikely (svptest_any (pg, special)))
    return special_case (u_off, hi, r2, y, special);
  return svmla_x (svptrue_b32 (), hi, r2, y);
}

TEST_SIG (SV, F, 1, log10, 0.01, 11.1)
TEST_ULP (SV_NAME_F1 (log10), 2.82)
TEST_DISABLE_FENV (SV_NAME_F1 (log10))
TEST_INTERVAL (SV_NAME_F1 (log10), -0.0, -0x1p126, 100)
TEST_INTERVAL (SV_NAME_F1 (log10), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (SV_NAME_F1 (log10), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_F1 (log10), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (SV_NAME_F1 (log10), 1.0, 100, 50000)
TEST_INTERVAL (SV_NAME_F1 (log10), 100, inf, 50000)
CLOSE_SVE_ATTR
