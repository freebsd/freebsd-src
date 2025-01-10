/*
 * Double-precision SVE log2 function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define N (1 << V_LOG2_TABLE_BITS)
#define Max (0x7ff0000000000000)
#define Min (0x0010000000000000)
#define Thresh (0x7fe0000000000000) /* Max - Min.  */

static const struct data
{
  double c0, c2;
  double c1, c3;
  double invln2, c4;
  uint64_t off;
} data = {
  .c0 = -0x1.71547652b83p-1,
  .c1 = 0x1.ec709dc340953p-2,
  .c2 = -0x1.71547651c8f35p-2,
  .c3 = 0x1.2777ebe12dda5p-2,
  .c4 = -0x1.ec738d616fe26p-3,
  .invln2 = 0x1.71547652b82fep0,
  .off = 0x3fe6900900000000,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t w, svuint64_t tmp, svfloat64_t y, svfloat64_t r2,
	      svbool_t special, const struct data *d)
{
  svfloat64_t x = svreinterpret_f64 (svadd_x (svptrue_b64 (), tmp, d->off));
  return sv_call_f64 (log2, x, svmla_x (svptrue_b64 (), w, r2, y), special);
}

/* Double-precision SVE log2 routine.
   Implements the same algorithm as AdvSIMD log10, with coefficients and table
   entries scaled in extended precision.
   The maximum observed error is 2.58 ULP:
   SV_NAME_D1 (log2)(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
					  want 0x1.fffb34198d9ddp-5.  */
svfloat64_t SV_NAME_D1 (log2) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, Min), Thresh);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  svuint64_t i = svlsr_x (pg, tmp, 51 - V_LOG2_TABLE_BITS);
  i = svand_x (pg, i, (N - 1) << 1);
  svfloat64_t k = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  svfloat64_t z = svreinterpret_f64 (
      svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52)));

  svfloat64_t invc = svld1_gather_index (pg, &__v_log2_data.table[0].invc, i);
  svfloat64_t log2c
      = svld1_gather_index (pg, &__v_log2_data.table[0].log2c, i);

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */

  svfloat64_t invln2_and_c4 = svld1rq_f64 (svptrue_b64 (), &d->invln2);
  svfloat64_t r = svmad_x (pg, invc, z, -1.0);
  svfloat64_t w = svmla_lane_f64 (log2c, r, invln2_and_c4, 0);
  w = svadd_x (pg, k, w);

  svfloat64_t odd_coeffs = svld1rq_f64 (svptrue_b64 (), &d->c1);
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t y = svmla_lane_f64 (sv_f64 (d->c2), r, odd_coeffs, 1);
  svfloat64_t p = svmla_lane_f64 (sv_f64 (d->c0), r, odd_coeffs, 0);
  y = svmla_lane_f64 (y, r2, invln2_and_c4, 1);
  y = svmla_x (pg, p, r2, y);

  if (unlikely (svptest_any (pg, special)))
    return special_case (w, tmp, y, r2, special, d);
  return svmla_x (pg, w, r2, y);
}

TEST_SIG (SV, D, 1, log2, 0.01, 11.1)
TEST_ULP (SV_NAME_D1 (log2), 2.09)
TEST_DISABLE_FENV (SV_NAME_D1 (log2))
TEST_INTERVAL (SV_NAME_D1 (log2), -0.0, -0x1p126, 1000)
TEST_INTERVAL (SV_NAME_D1 (log2), 0.0, 0x1p-126, 4000)
TEST_INTERVAL (SV_NAME_D1 (log2), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_D1 (log2), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (SV_NAME_D1 (log2), 1.0, 100, 50000)
TEST_INTERVAL (SV_NAME_D1 (log2), 100, inf, 50000)
CLOSE_SVE_ATTR
