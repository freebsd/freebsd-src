/*
 * Single-precision vector/SVE log2 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define P(i) __v_log2f_data.poly[i]

#define Ln2 (0x1.62e43p-1f) /* 0x3f317218.  */
#define Min (0x00800000)
#define Max (0x7f800000)
#define Mask (0x007fffff)
#define Off (0x3f2aaaab) /* 0.666667.  */

static NOINLINE sv_f32_t
specialcase (sv_f32_t x, sv_f32_t y, svbool_t cmp)
{
  return sv_call_f32 (log2f, x, y, cmp);
}

/* Optimised implementation of SVE log2f, using the same algorithm
   and polynomial as Neon log2f. Maximum error is 2.48 ULPs:
   __sv_log2f(0x1.558174p+0) got 0x1.a9be84p-2
			    want 0x1.a9be8p-2.  */
sv_f32_t
__sv_log2f_x (sv_f32_t x, const svbool_t pg)
{
  sv_u32_t u = sv_as_u32_f32 (x);
  svbool_t special
    = svcmpge_u32 (pg, svsub_n_u32_x (pg, u, Min), sv_u32 (Max - Min));

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = svsub_n_u32_x (pg, u, Off);
  sv_f32_t n = sv_to_f32_s32_x (pg, svasr_n_s32_x (pg, sv_as_s32_u32 (u),
						   23)); /* Sign-extend.  */
  u = svand_n_u32_x (pg, u, Mask);
  u = svadd_n_u32_x (pg, u, Off);
  sv_f32_t r = svsub_n_f32_x (pg, sv_as_f32_u32 (u), 1.0f);

  /* y = log2(1+r) + n.  */
  sv_f32_t r2 = svmul_f32_x (pg, r, r);

  /* Evaluate polynomial using pairwise Horner scheme.  */
  sv_f32_t p67 = sv_fma_n_f32_x (pg, P (7), r, sv_f32 (P (6)));
  sv_f32_t p45 = sv_fma_n_f32_x (pg, P (5), r, sv_f32 (P (4)));
  sv_f32_t p23 = sv_fma_n_f32_x (pg, P (3), r, sv_f32 (P (2)));
  sv_f32_t p01 = sv_fma_n_f32_x (pg, P (1), r, sv_f32 (P (0)));
  sv_f32_t y;
  y = sv_fma_n_f32_x (pg, P (8), r2, p67);
  y = sv_fma_f32_x (pg, y, r2, p45);
  y = sv_fma_f32_x (pg, y, r2, p23);
  y = sv_fma_f32_x (pg, y, r2, p01);
  y = sv_fma_f32_x (pg, y, r, n);

  if (unlikely (svptest_any (pg, special)))
    return specialcase (x, y, special);
  return y;
}

PL_ALIAS (__sv_log2f_x, _ZGVsMxv_log2f)

PL_SIG (SV, F, 1, log2, 0.01, 11.1)
PL_TEST_ULP (__sv_log2f, 1.99)
PL_TEST_EXPECT_FENV_ALWAYS (__sv_log2f)
PL_TEST_INTERVAL (__sv_log2f, -0.0, -0x1p126, 4000)
PL_TEST_INTERVAL (__sv_log2f, 0.0, 0x1p-126, 4000)
PL_TEST_INTERVAL (__sv_log2f, 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (__sv_log2f, 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (__sv_log2f, 1.0, 100, 50000)
PL_TEST_INTERVAL (__sv_log2f, 100, inf, 50000)

#endif // SV_SUPPORTED
