/*
 * Single-precision SVE 10^x function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#define _GNU_SOURCE
#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_poly_f32.h"

/* For x < -Thres, the result is subnormal and not handled correctly by
   FEXPA.  */
#define Thres 37.9

static const struct data
{
  float log2_10_lo, c0, c2, c4;
  float c1, c3, log10_2;
  float shift, log2_10_hi, thres;
} data = {
  /* Coefficients generated using Remez algorithm with minimisation of relative
     error.
     rel error: 0x1.89dafa3p-24
     abs error: 0x1.167d55p-23 in [-log10(2)/2, log10(2)/2]
     maxerr: 0.52 +0.5 ulp.  */
  .c0 = 0x1.26bb16p+1f,
  .c1 = 0x1.5350d2p+1f,
  .c2 = 0x1.04744ap+1f,
  .c3 = 0x1.2d8176p+0f,
  .c4 = 0x1.12b41ap-1f,
  /* 1.5*2^17 + 127, a shift value suitable for FEXPA.  */
  .shift = 0x1.803f8p17f,
  .log10_2 = 0x1.a934fp+1,
  .log2_10_hi = 0x1.344136p-2,
  .log2_10_lo = -0x1.ec10cp-27,
  .thres = Thres,
};

static inline svfloat32_t
sv_exp10f_inline (svfloat32_t x, const svbool_t pg, const struct data *d)
{
  /* exp10(x) = 2^(n/N) * 10^r = 2^n * (1 + poly (r)),
     with poly(r) in [1/sqrt(2), sqrt(2)] and
     x = r + n * log10(2) / N, with r in [-log10(2)/2N, log10(2)/2N].  */

  svfloat32_t lane_consts = svld1rq (svptrue_b32 (), &d->log2_10_lo);

  /* n = round(x/(log10(2)/N)).  */
  svfloat32_t shift = sv_f32 (d->shift);
  svfloat32_t z = svmad_x (pg, sv_f32 (d->log10_2), x, shift);
  svfloat32_t n = svsub_x (svptrue_b32 (), z, shift);

  /* r = x - n*log10(2)/N.  */
  svfloat32_t r = svmsb_x (pg, sv_f32 (d->log2_10_hi), n, x);
  r = svmls_lane (r, n, lane_consts, 0);

  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* Polynomial evaluation: poly(r) ~ exp10(r)-1.  */
  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), r, lane_consts, 2);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), r, lane_consts, 3);
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t p14 = svmla_x (pg, p12, p34, r2);
  svfloat32_t p0 = svmul_lane (r, lane_consts, 1);
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  return svmla_x (pg, scale, scale, poly);
}

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t special, const struct data *d)
{
  return sv_call_f32 (exp10f, x, sv_exp10f_inline (x, svptrue_b32 (), d),
		      special);
}

/* Single-precision SVE exp10f routine. Implements the same algorithm
   as AdvSIMD exp10f.
   Worst case error is 1.02 ULPs.
   _ZGVsMxv_exp10f(-0x1.040488p-4) got 0x1.ba5f9ep-1
				  want 0x1.ba5f9cp-1.  */
svfloat32_t SV_NAME_F1 (exp10) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t special = svacgt (pg, x, d->thres);
  if (unlikely (svptest_any (special, special)))
    return special_case (x, special, d);
  return sv_exp10f_inline (x, pg, d);
}

#if WANT_EXP10_TESTS
TEST_SIG (SV, F, 1, exp10, -9.9, 9.9)
TEST_ULP (SV_NAME_F1 (exp10), 0.52)
TEST_DISABLE_FENV (SV_NAME_F1 (exp10))
TEST_SYM_INTERVAL (SV_NAME_F1 (exp10), 0, Thres, 50000)
TEST_SYM_INTERVAL (SV_NAME_F1 (exp10), Thres, inf, 50000)
#endif
CLOSE_SVE_ATTR
