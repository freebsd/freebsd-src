/*
 * Single-precision SVE 2^x function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define Thres 0x1.5d5e2ap+6f

static const struct data
{
  float c0, c2, c4, c1, c3;
  float shift, thres;
} data = {
  /* Coefficients copied from the polynomial in AdvSIMD variant.  */
  .c0 = 0x1.62e422p-1f,
  .c1 = 0x1.ebf9bcp-3f,
  .c2 = 0x1.c6bd32p-5f,
  .c3 = 0x1.3ce9e4p-7f,
  .c4 = 0x1.59977ap-10f,
  /* 1.5*2^17 + 127.  */
  .shift = 0x1.803f8p17f,
  /* Roughly 87.3. For x < -Thres, the result is subnormal and not handled
     correctly by FEXPA.  */
  .thres = Thres,
};

static inline svfloat32_t
sv_exp2f_inline (svfloat32_t x, const svbool_t pg, const struct data *d)
{
  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
    x = n + r, with r in [-1/2, 1/2].  */
  svfloat32_t z = svadd_x (svptrue_b32 (), x, d->shift);
  svfloat32_t n = svsub_x (svptrue_b32 (), z, d->shift);
  svfloat32_t r = svsub_x (svptrue_b32 (), x, n);

  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* Polynomial evaluation: poly(r) ~ exp2(r)-1.
     Evaluate polynomial use hybrid scheme - offset ESTRIN by 1 for
     coefficients 1 to 4, and apply most significant coefficient directly.  */
  svfloat32_t even_coeffs = svld1rq (svptrue_b32 (), &d->c0);
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), r, even_coeffs, 1);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), r, even_coeffs, 2);
  svfloat32_t p14 = svmla_x (pg, p12, r2, p34);
  svfloat32_t p0 = svmul_lane (r, even_coeffs, 0);
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  return svmla_x (pg, scale, scale, poly);
}

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t special, const struct data *d)
{
  return sv_call_f32 (exp2f, x, sv_exp2f_inline (x, svptrue_b32 (), d),
		      special);
}

/* Single-precision SVE exp2f routine. Implements the same algorithm
   as AdvSIMD exp2f.
   Worst case error is 1.04 ULPs.
   _ZGVsMxv_exp2f(-0x1.af994ap-3) got 0x1.ba6a66p-1
				 want 0x1.ba6a64p-1.  */
svfloat32_t SV_NAME_F1 (exp2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t special = svacgt (pg, x, d->thres);
  if (unlikely (svptest_any (special, special)))
    return special_case (x, special, d);
  return sv_exp2f_inline (x, pg, d);
}

TEST_SIG (SV, F, 1, exp2, -9.9, 9.9)
TEST_ULP (SV_NAME_F1 (exp2), 0.54)
TEST_DISABLE_FENV (SV_NAME_F1 (exp2))
TEST_SYM_INTERVAL (SV_NAME_F1 (exp2), 0, Thres, 50000)
TEST_SYM_INTERVAL (SV_NAME_F1 (exp2), Thres, inf, 50000)
CLOSE_SVE_ATTR
