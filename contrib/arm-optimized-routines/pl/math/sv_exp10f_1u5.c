/*
 * Single-precision SVE 2^x function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "include/mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f32.h"

/* For x < -SpecialBound, the result is subnormal and not handled correctly by
   FEXPA.  */
#define SpecialBound 37.9

static const struct data
{
  float poly[5];
  float shift, log10_2, log2_10_hi, log2_10_lo, special_bound;
} data = {
  /* Coefficients generated using Remez algorithm with minimisation of relative
     error.
     rel error: 0x1.89dafa3p-24
     abs error: 0x1.167d55p-23 in [-log10(2)/2, log10(2)/2]
     maxerr: 0.52 +0.5 ulp.  */
  .poly = { 0x1.26bb16p+1f, 0x1.5350d2p+1f, 0x1.04744ap+1f, 0x1.2d8176p+0f,
	    0x1.12b41ap-1f },
  /* 1.5*2^17 + 127, a shift value suitable for FEXPA.  */
  .shift = 0x1.903f8p17f,
  .log10_2 = 0x1.a934fp+1,
  .log2_10_hi = 0x1.344136p-2,
  .log2_10_lo = -0x1.ec10cp-27,
  .special_bound = SpecialBound,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (exp10f, x, y, special);
}

/* Single-precision SVE exp10f routine. Implements the same algorithm
   as AdvSIMD exp10f.
   Worst case error is 1.02 ULPs.
   _ZGVsMxv_exp10f(-0x1.040488p-4) got 0x1.ba5f9ep-1
				  want 0x1.ba5f9cp-1.  */
svfloat32_t SV_NAME_F1 (exp10) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  /* exp10(x) = 2^(n/N) * 10^r = 2^n * (1 + poly (r)),
     with poly(r) in [1/sqrt(2), sqrt(2)] and
     x = r + n * log10(2) / N, with r in [-log10(2)/2N, log10(2)/2N].  */

  /* Load some constants in quad-word chunks to minimise memory access (last
     lane is wasted).  */
  svfloat32_t log10_2_and_inv = svld1rq (svptrue_b32 (), &d->log10_2);

  /* n = round(x/(log10(2)/N)).  */
  svfloat32_t shift = sv_f32 (d->shift);
  svfloat32_t z = svmla_lane (shift, x, log10_2_and_inv, 0);
  svfloat32_t n = svsub_x (pg, z, shift);

  /* r = x - n*log10(2)/N.  */
  svfloat32_t r = svmls_lane (x, n, log10_2_and_inv, 1);
  r = svmls_lane (r, n, log10_2_and_inv, 2);

  svbool_t special = svacgt (pg, x, d->special_bound);
  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* Polynomial evaluation: poly(r) ~ exp10(r)-1.  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t poly
      = svmla_x (pg, svmul_x (pg, r, d->poly[0]),
		 sv_pairwise_poly_3_f32_x (pg, r, r2, d->poly + 1), r2);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svmla_x (pg, scale, scale, poly), special);

  return svmla_x (pg, scale, scale, poly);
}

PL_SIG (SV, F, 1, exp10, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_F1 (exp10), 0.52)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (exp10), 0, SpecialBound, 50000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (exp10), SpecialBound, inf, 50000)
