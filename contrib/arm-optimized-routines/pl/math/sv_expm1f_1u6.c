/*
 * Single-precision vector exp(x) - 1 function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

/* Largest value of x for which expm1(x) should round to -1.  */
#define SpecialBound 0x1.5ebc4p+6f

static const struct data
{
  /* These 4 are grouped together so they can be loaded as one quadword, then
     used with _lane forms of svmla/svmls.  */
  float c2, c4, ln2_hi, ln2_lo;
  float c0, c1, c3, inv_ln2, special_bound, shift;
} data = {
  /* Generated using fpminimax.  */
  .c0 = 0x1.fffffep-2,		 .c1 = 0x1.5554aep-3,
  .c2 = 0x1.555736p-5,		 .c3 = 0x1.12287cp-7,
  .c4 = 0x1.6b55a2p-10,

  .special_bound = SpecialBound, .shift = 0x1.8p23f,
  .inv_ln2 = 0x1.715476p+0f,	 .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,
};

#define C(i) sv_f32 (d->c##i)

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t pg)
{
  return sv_call_f32 (expm1f, x, x, pg);
}

/* Single-precision SVE exp(x) - 1. Maximum error is 1.52 ULP:
   _ZGVsMxv_expm1f(0x1.8f4ebcp-2) got 0x1.e859dp-2
				 want 0x1.e859d4p-2.  */
svfloat32_t SV_NAME_F1 (expm1) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Large, NaN/Inf.  */
  svbool_t special = svnot_z (pg, svaclt (pg, x, d->special_bound));

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, pg);

  /* This vector is reliant on layout of data - it contains constants
     that can be used with _lane forms of svmla/svmls. Values are:
     [ coeff_2, coeff_4, ln2_hi, ln2_lo ].  */
  svfloat32_t lane_constants = svld1rq (svptrue_b32 (), &d->c2);

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  svfloat32_t j = svmla_x (pg, sv_f32 (d->shift), x, d->inv_ln2);
  j = svsub_x (pg, j, d->shift);
  svint32_t i = svcvt_s32_x (pg, j);

  svfloat32_t f = svmls_lane (x, j, lane_constants, 2);
  f = svmls_lane (f, j, lane_constants, 3);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  svfloat32_t p12 = svmla_lane (C (1), f, lane_constants, 0);
  svfloat32_t p34 = svmla_lane (C (3), f, lane_constants, 1);
  svfloat32_t f2 = svmul_x (pg, f, f);
  svfloat32_t p = svmla_x (pg, p12, f2, p34);
  p = svmla_x (pg, C (0), f, p);
  p = svmla_x (pg, f, f2, p);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  svfloat32_t t = svreinterpret_f32 (
      svadd_x (pg, svreinterpret_u32 (svlsl_x (pg, i, 23)), 0x3f800000));
  return svmla_x (pg, svsub_x (pg, t, 1), p, t);
}

PL_SIG (SV, F, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_F1 (expm1), 1.02)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (expm1), 0, SpecialBound, 100000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (expm1), SpecialBound, inf, 1000)
