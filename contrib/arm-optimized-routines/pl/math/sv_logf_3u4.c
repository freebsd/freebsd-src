/*
 * Single-precision vector log function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define P(i) __sv_logf_poly[i]

#define Ln2 (0x1.62e43p-1f) /* 0x3f317218 */
#define Min (0x00800000)
#define Max (0x7f800000)
#define Mask (0x007fffff)
#define Off (0x3f2aaaab) /* 0.666667 */

float
optr_aor_log_f32 (float);

static NOINLINE sv_f32_t
__sv_logf_specialcase (sv_f32_t x, sv_f32_t y, svbool_t cmp)
{
  return sv_call_f32 (optr_aor_log_f32, x, y, cmp);
}

/* Optimised implementation of SVE logf, using the same algorithm and polynomial
   as the Neon routine in math/. Maximum error is 3.34 ULPs:
   __sv_logf(0x1.557298p+0) got 0x1.26edecp-2
			   want 0x1.26ede6p-2.  */
sv_f32_t
__sv_logf_x (sv_f32_t x, const svbool_t pg)
{
  sv_u32_t u = sv_as_u32_f32 (x);
  svbool_t cmp
    = svcmpge_u32 (pg, svsub_n_u32_x (pg, u, Min), sv_u32 (Max - Min));

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = svsub_n_u32_x (pg, u, Off);
  sv_f32_t n = sv_to_f32_s32_x (pg, svasr_n_s32_x (pg, sv_as_s32_u32 (u),
						   23)); /* Sign-extend.  */
  u = svand_n_u32_x (pg, u, Mask);
  u = svadd_n_u32_x (pg, u, Off);
  sv_f32_t r = svsub_n_f32_x (pg, sv_as_f32_u32 (u), 1.0f);

  /* y = log(1+r) + n*ln2.  */
  sv_f32_t r2 = svmul_f32_x (pg, r, r);
  /* n*ln2 + r + r2*(P6 + r*P5 + r2*(P4 + r*P3 + r2*(P2 + r*P1 + r2*P0))).  */
  sv_f32_t p = sv_fma_n_f32_x (pg, P (1), r, sv_f32 (P (2)));
  sv_f32_t q = sv_fma_n_f32_x (pg, P (3), r, sv_f32 (P (4)));
  sv_f32_t y = sv_fma_n_f32_x (pg, P (5), r, sv_f32 (P (6)));
  p = sv_fma_n_f32_x (pg, P (0), r2, p);
  q = sv_fma_f32_x (pg, p, r2, q);
  y = sv_fma_f32_x (pg, q, r2, y);
  p = sv_fma_n_f32_x (pg, Ln2, n, r);
  y = sv_fma_f32_x (pg, y, r2, p);

  if (unlikely (svptest_any (pg, cmp)))
    return __sv_logf_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_logf_x, _ZGVsMxv_logf)

PL_SIG (SV, F, 1, log, 0.01, 11.1)
PL_TEST_ULP (__sv_logf, 2.85)
PL_TEST_INTERVAL (__sv_logf, -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (__sv_logf, 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (__sv_logf, 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (__sv_logf, 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (__sv_logf, 1.0, 100, 50000)
PL_TEST_INTERVAL (__sv_logf, 100, inf, 50000)
#endif // SV_SUPPORTED
