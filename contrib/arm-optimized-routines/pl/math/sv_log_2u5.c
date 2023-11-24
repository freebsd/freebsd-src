/*
 * Double-precision SVE log(x) function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define A(i) __sv_log_data.poly[i]
#define Ln2 (0x1.62e42fefa39efp-1)
#define N (1 << SV_LOG_TABLE_BITS)
#define OFF (0x3fe6900900000000)

double
optr_aor_log_f64 (double);

static NOINLINE sv_f64_t
__sv_log_specialcase (sv_f64_t x, sv_f64_t y, svbool_t cmp)
{
  return sv_call_f64 (optr_aor_log_f64, x, y, cmp);
}

/* SVE port of Neon log algorithm from math/.
   Maximum measured error is 2.17 ulp:
   __sv_log(0x1.a6129884398a3p+0) got 0x1.ffffff1cca043p-2
				 want 0x1.ffffff1cca045p-2.  */
sv_f64_t
__sv_log_x (sv_f64_t x, const svbool_t pg)
{
  sv_u64_t ix = sv_as_u64_f64 (x);
  sv_u64_t top = svlsr_n_u64_x (pg, ix, 48);
  svbool_t cmp = svcmpge_u64 (pg, svsub_n_u64_x (pg, top, 0x0010),
			      sv_u64 (0x7ff0 - 0x0010));

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  sv_u64_t tmp = svsub_n_u64_x (pg, ix, OFF);
  /* Equivalent to (tmp >> (52 - SV_LOG_TABLE_BITS)) % N, since N is a power
     of 2.  */
  sv_u64_t i
    = svand_n_u64_x (pg, svlsr_n_u64_x (pg, tmp, (52 - SV_LOG_TABLE_BITS)),
		     N - 1);
  sv_s64_t k
    = svasr_n_s64_x (pg, sv_as_s64_u64 (tmp), 52); /* Arithmetic shift.  */
  sv_u64_t iz = svsub_u64_x (pg, ix, svand_n_u64_x (pg, tmp, 0xfffULL << 52));
  sv_f64_t z = sv_as_f64_u64 (iz);
  /* Lookup in 2 global lists (length N).  */
  sv_f64_t invc = sv_lookup_f64_x (pg, __sv_log_data.invc, i);
  sv_f64_t logc = sv_lookup_f64_x (pg, __sv_log_data.logc, i);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  sv_f64_t r = sv_fma_f64_x (pg, z, invc, sv_f64 (-1.0));
  sv_f64_t kd = sv_to_f64_s64_x (pg, k);
  /* hi = r + log(c) + k*Ln2.  */
  sv_f64_t hi = sv_fma_n_f64_x (pg, Ln2, kd, svadd_f64_x (pg, logc, r));
  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  sv_f64_t r2 = svmul_f64_x (pg, r, r);
  sv_f64_t y = sv_fma_n_f64_x (pg, A (3), r, sv_f64 (A (2)));
  sv_f64_t p = sv_fma_n_f64_x (pg, A (1), r, sv_f64 (A (0)));
  y = sv_fma_n_f64_x (pg, A (4), r2, y);
  y = sv_fma_f64_x (pg, y, r2, p);
  y = sv_fma_f64_x (pg, y, r2, hi);

  if (unlikely (svptest_any (pg, cmp)))
    return __sv_log_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_log_x, _ZGVsMxv_log)

PL_SIG (SV, D, 1, log, 0.01, 11.1)
PL_TEST_ULP (__sv_log, 1.68)
PL_TEST_INTERVAL (__sv_log, -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (__sv_log, 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (__sv_log, 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (__sv_log, 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (__sv_log, 1.0, 100, 50000)
PL_TEST_INTERVAL (__sv_log, 100, inf, 50000)
#endif // SV_SUPPORTED
