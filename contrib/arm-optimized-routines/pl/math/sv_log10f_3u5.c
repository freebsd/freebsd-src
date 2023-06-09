/*
 * Single-precision SVE log10 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define SpecialCaseMin 0x00800000
#define SpecialCaseMax 0x7f800000
#define Offset 0x3f2aaaab /* 0.666667.  */
#define Mask 0x007fffff
#define Ln2 0x1.62e43p-1f /* 0x3f317218.  */
#define InvLn10 0x1.bcb7b2p-2f

#define P(i) __v_log10f_poly[i]

static NOINLINE sv_f32_t
special_case (sv_f32_t x, sv_f32_t y, svbool_t special)
{
  return sv_call_f32 (log10f, x, y, special);
}

/* Optimised implementation of SVE log10f using the same algorithm and
   polynomial as v_log10f. Maximum error is 3.31ulps:
   __sv_log10f(0x1.555c16p+0) got 0x1.ffe2fap-4
			     want 0x1.ffe2f4p-4.  */
sv_f32_t
__sv_log10f_x (sv_f32_t x, const svbool_t pg)
{
  sv_u32_t ix = sv_as_u32_f32 (x);
  svbool_t special_cases
    = svcmpge_n_u32 (pg, svsub_n_u32_x (pg, ix, SpecialCaseMin),
		     SpecialCaseMax - SpecialCaseMin);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  ix = svsub_n_u32_x (pg, ix, Offset);
  sv_f32_t n = sv_to_f32_s32_x (pg, svasr_n_s32_x (pg, sv_as_s32_u32 (ix),
						   23)); /* signextend.  */
  ix = svand_n_u32_x (pg, ix, Mask);
  ix = svadd_n_u32_x (pg, ix, Offset);
  sv_f32_t r = svsub_n_f32_x (pg, sv_as_f32_u32 (ix), 1.0f);

  /* y = log10(1+r) + n*log10(2)
     log10(1+r) ~ r * InvLn(10) + P(r)
     where P(r) is a polynomial. Use order 9 for log10(1+x), i.e. order 8 for
     log10(1+x)/x, with x in [-1/3, 1/3] (offset=2/3)

     P(r) = r2 * (Q01 + r2 * (Q23 + r2 * (Q45 + r2 * Q67)))
     and Qij  = Pi + r * Pj.  */
  sv_f32_t q12 = sv_fma_n_f32_x (pg, P (1), r, sv_f32 (P (0)));
  sv_f32_t q34 = sv_fma_n_f32_x (pg, P (3), r, sv_f32 (P (2)));
  sv_f32_t q56 = sv_fma_n_f32_x (pg, P (5), r, sv_f32 (P (4)));
  sv_f32_t q78 = sv_fma_n_f32_x (pg, P (7), r, sv_f32 (P (6)));

  sv_f32_t r2 = svmul_f32_x (pg, r, r);
  sv_f32_t y = sv_fma_f32_x (pg, q78, r2, q56);
  y = sv_fma_f32_x (pg, y, r2, q34);
  y = sv_fma_f32_x (pg, y, r2, q12);

  /* Using p = Log10(2)*n + r*InvLn(10) is slightly faster but less
     accurate.  */
  sv_f32_t p = sv_fma_n_f32_x (pg, Ln2, n, r);
  y = sv_fma_f32_x (pg, y, r2, svmul_n_f32_x (pg, p, InvLn10));

  if (unlikely (svptest_any (pg, special_cases)))
    {
      return special_case (x, y, special_cases);
    }
  return y;
}

PL_ALIAS (__sv_log10f_x, _ZGVsMxv_log10f)

PL_SIG (SV, F, 1, log10, 0.01, 11.1)
PL_TEST_ULP (__sv_log10f, 2.82)
PL_TEST_INTERVAL (__sv_log10f, -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (__sv_log10f, 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (__sv_log10f, 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (__sv_log10f, 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (__sv_log10f, 1.0, 100, 50000)
PL_TEST_INTERVAL (__sv_log10f, 100, inf, 50000)
#endif
