/*
 * Double-precision SVE log2 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define InvLn2 sv_f64 (0x1.71547652b82fep0)
#define N (1 << V_LOG2_TABLE_BITS)
#define OFF 0x3fe6900900000000
#define P(i) sv_f64 (__v_log2_data.poly[i])

NOINLINE static sv_f64_t
specialcase (sv_f64_t x, sv_f64_t y, const svbool_t cmp)
{
  return sv_call_f64 (log2, x, y, cmp);
}

/* Double-precision SVE log2 routine. Implements the same algorithm as vector
   log10, with coefficients and table entries scaled in extended precision.
   The maximum observed error is 2.58 ULP:
   __v_log2(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
				 want 0x1.fffb34198d9ddp-5.  */
sv_f64_t
__sv_log2_x (sv_f64_t x, const svbool_t pg)
{
  sv_u64_t ix = sv_as_u64_f64 (x);
  sv_u64_t top = svlsr_n_u64_x (pg, ix, 48);

  svbool_t special
    = svcmpge_n_u64 (pg, svsub_n_u64_x (pg, top, 0x0010), 0x7ff0 - 0x0010);

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  sv_u64_t tmp = svsub_n_u64_x (pg, ix, OFF);
  sv_u64_t i
    = sv_mod_n_u64_x (pg, svlsr_n_u64_x (pg, tmp, 52 - V_LOG2_TABLE_BITS), N);
  sv_f64_t k
    = sv_to_f64_s64_x (pg, svasr_n_s64_x (pg, sv_as_s64_u64 (tmp), 52));
  sv_f64_t z = sv_as_f64_u64 (
    svsub_u64_x (pg, ix, svand_n_u64_x (pg, tmp, 0xfffULL << 52)));

  sv_u64_t idx = svmul_n_u64_x (pg, i, 2);
  sv_f64_t invc = sv_lookup_f64_x (pg, &__v_log2_data.tab[0].invc, idx);
  sv_f64_t log2c = sv_lookup_f64_x (pg, &__v_log2_data.tab[0].log2c, idx);

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */

  sv_f64_t r = sv_fma_f64_x (pg, z, invc, sv_f64 (-1.0));
  sv_f64_t w = sv_fma_f64_x (pg, r, InvLn2, log2c);

  sv_f64_t r2 = svmul_f64_x (pg, r, r);
  sv_f64_t p_23 = sv_fma_f64_x (pg, P (3), r, P (2));
  sv_f64_t p_01 = sv_fma_f64_x (pg, P (1), r, P (0));
  sv_f64_t y = sv_fma_f64_x (pg, P (4), r2, p_23);
  y = sv_fma_f64_x (pg, y, r2, p_01);
  y = sv_fma_f64_x (pg, y, r2, svadd_f64_x (pg, k, w));

  if (unlikely (svptest_any (pg, special)))
    {
      return specialcase (x, y, special);
    }
  return y;
}

PL_ALIAS (__sv_log2_x, _ZGVsMxv_log2)

PL_SIG (SV, D, 1, log2, 0.01, 11.1)
PL_TEST_ULP (__sv_log2, 2.09)
PL_TEST_EXPECT_FENV_ALWAYS (__sv_log2)
PL_TEST_INTERVAL (__sv_log2, -0.0, -0x1p126, 1000)
PL_TEST_INTERVAL (__sv_log2, 0.0, 0x1p-126, 4000)
PL_TEST_INTERVAL (__sv_log2, 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (__sv_log2, 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (__sv_log2, 1.0, 100, 50000)
PL_TEST_INTERVAL (__sv_log2, 100, inf, 50000)

#endif
