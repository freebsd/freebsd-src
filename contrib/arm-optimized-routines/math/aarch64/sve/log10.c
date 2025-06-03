/*
 * Double-precision SVE log10(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define Min 0x0010000000000000
#define Max 0x7ff0000000000000
#define Thres 0x7fe0000000000000 /* Max - Min.  */
#define N (1 << V_LOG10_TABLE_BITS)

static const struct data
{
  double c0, c2;
  double c1, c3;
  double invln10, log10_2;
  double c4;
  uint64_t off;
} data = {
  .c0 = -0x1.bcb7b1526e506p-3,
  .c1 = 0x1.287a7636be1d1p-3,
  .c2 = -0x1.bcb7b158af938p-4,
  .c3 = 0x1.63c78734e6d07p-4,
  .c4 = -0x1.287461742fee4p-4,
  .invln10 = 0x1.bcb7b1526e50ep-2,
  .log10_2 = 0x1.34413509f79ffp-2,
  .off = 0x3fe6900900000000,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t hi, svuint64_t tmp, svfloat64_t y, svfloat64_t r2,
	      svbool_t special, const struct data *d)
{
  svfloat64_t x = svreinterpret_f64 (svadd_x (svptrue_b64 (), tmp, d->off));
  return sv_call_f64 (log10, x, svmla_x (svptrue_b64 (), hi, r2, y), special);
}

/* Double-precision SVE log10 routine.
   Maximum measured error is 2.46 ulps.
   SV_NAME_D1 (log10)(0x1.131956cd4b627p+0) got 0x1.fffbdf6eaa669p-6
					   want 0x1.fffbdf6eaa667p-6.  */
svfloat64_t SV_NAME_D1 (log10) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, Min), Thres);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  svuint64_t i = svlsr_x (pg, tmp, 51 - V_LOG10_TABLE_BITS);
  i = svand_x (pg, i, (N - 1) << 1);
  svfloat64_t k = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  svfloat64_t z = svreinterpret_f64 (
      svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52)));

  /* log(x) = k*log(2) + log(c) + log(z/c).  */
  svfloat64_t invc = svld1_gather_index (pg, &__v_log10_data.table[0].invc, i);
  svfloat64_t logc
      = svld1_gather_index (pg, &__v_log10_data.table[0].log10c, i);

  /* We approximate log(z/c) with a polynomial P(x) ~= log(x + 1):
     r = z/c - 1 (we look up precomputed 1/c)
     log(z/c) ~= P(r).  */
  svfloat64_t r = svmad_x (pg, invc, z, -1.0);

  /* hi = log(c) + k*log(2).  */
  svfloat64_t invln10_log10_2 = svld1rq_f64 (svptrue_b64 (), &d->invln10);
  svfloat64_t w = svmla_lane_f64 (logc, r, invln10_log10_2, 0);
  svfloat64_t hi = svmla_lane_f64 (w, k, invln10_log10_2, 1);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  svfloat64_t odd_coeffs = svld1rq_f64 (svptrue_b64 (), &d->c1);
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t y = svmla_lane_f64 (sv_f64 (d->c2), r, odd_coeffs, 1);
  svfloat64_t p = svmla_lane_f64 (sv_f64 (d->c0), r, odd_coeffs, 0);
  y = svmla_x (pg, y, r2, d->c4);
  y = svmla_x (pg, p, r2, y);

  if (unlikely (svptest_any (pg, special)))
    return special_case (hi, tmp, y, r2, special, d);
  return svmla_x (pg, hi, r2, y);
}

TEST_SIG (SV, D, 1, log10, 0.01, 11.1)
TEST_ULP (SV_NAME_D1 (log10), 1.97)
TEST_DISABLE_FENV (SV_NAME_D1 (log10))
TEST_INTERVAL (SV_NAME_D1 (log10), -0.0, -0x1p126, 100)
TEST_INTERVAL (SV_NAME_D1 (log10), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (SV_NAME_D1 (log10), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_D1 (log10), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (SV_NAME_D1 (log10), 1.0, 100, 50000)
TEST_INTERVAL (SV_NAME_D1 (log10), 100, inf, 50000)
CLOSE_SVE_ATTR
