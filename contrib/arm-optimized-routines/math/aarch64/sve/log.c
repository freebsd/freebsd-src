/*
 * Double-precision SVE log(x) function.
 *
 * Copyright (c) 2020-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define N (1 << V_LOG_TABLE_BITS)
#define Max (0x7ff0000000000000)
#define Min (0x0010000000000000)
#define Thresh (0x7fe0000000000000) /* Max - Min.  */

static const struct data
{
  double c0, c2;
  double c1, c3;
  double ln2, c4;
  uint64_t off;
} data = {
  .c0 = -0x1.ffffffffffff7p-2,
  .c1 = 0x1.55555555170d4p-2,
  .c2 = -0x1.0000000399c27p-2,
  .c3 = 0x1.999b2e90e94cap-3,
  .c4 = -0x1.554e550bd501ep-3,
  .ln2 = 0x1.62e42fefa39efp-1,
  .off = 0x3fe6900900000000,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t hi, svuint64_t tmp, svfloat64_t y, svfloat64_t r2,
	      svbool_t special, const struct data *d)
{
  svfloat64_t x = svreinterpret_f64 (svadd_x (svptrue_b64 (), tmp, d->off));
  return sv_call_f64 (log, x, svmla_x (svptrue_b64 (), hi, r2, y), special);
}

/* Double-precision SVE log routine.
   Maximum measured error is 2.64 ulp:
   SV_NAME_D1 (log)(0x1.95e54bc91a5e2p+184) got 0x1.fffffffe88cacp+6
					   want 0x1.fffffffe88cafp+6.  */
svfloat64_t SV_NAME_D1 (log) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, Min), Thresh);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  /* Calculate table index = (tmp >> (52 - V_LOG_TABLE_BITS)) % N.
     The actual value of i is double this due to table layout.  */
  svuint64_t i
      = svand_x (pg, svlsr_x (pg, tmp, (51 - V_LOG_TABLE_BITS)), (N - 1) << 1);
  svuint64_t iz = svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52));
  svfloat64_t z = svreinterpret_f64 (iz);
  /* Lookup in 2 global lists (length N).  */
  svfloat64_t invc = svld1_gather_index (pg, &__v_log_data.table[0].invc, i);
  svfloat64_t logc = svld1_gather_index (pg, &__v_log_data.table[0].logc, i);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  svfloat64_t kd = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  /* hi = r + log(c) + k*Ln2.  */
  svfloat64_t ln2_and_c4 = svld1rq_f64 (svptrue_b64 (), &d->ln2);
  svfloat64_t r = svmad_x (pg, invc, z, -1);
  svfloat64_t hi = svmla_lane_f64 (logc, kd, ln2_and_c4, 0);
  hi = svadd_x (pg, r, hi);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  svfloat64_t odd_coeffs = svld1rq_f64 (svptrue_b64 (), &d->c1);
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t y = svmla_lane_f64 (sv_f64 (d->c2), r, odd_coeffs, 1);
  svfloat64_t p = svmla_lane_f64 (sv_f64 (d->c0), r, odd_coeffs, 0);
  y = svmla_lane_f64 (y, r2, ln2_and_c4, 1);
  y = svmla_x (pg, p, r2, y);

  if (unlikely (svptest_any (pg, special)))
    return special_case (hi, tmp, y, r2, special, d);
  return svmla_x (pg, hi, r2, y);
}

TEST_SIG (SV, D, 1, log, 0.01, 11.1)
TEST_ULP (SV_NAME_D1 (log), 2.15)
TEST_DISABLE_FENV (SV_NAME_D1 (log))
TEST_INTERVAL (SV_NAME_D1 (log), -0.0, -inf, 1000)
TEST_INTERVAL (SV_NAME_D1 (log), 0, 0x1p-149, 1000)
TEST_INTERVAL (SV_NAME_D1 (log), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (SV_NAME_D1 (log), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_D1 (log), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (SV_NAME_D1 (log), 1.0, 100, 50000)
TEST_INTERVAL (SV_NAME_D1 (log), 100, inf, 50000)
CLOSE_SVE_ATTR
