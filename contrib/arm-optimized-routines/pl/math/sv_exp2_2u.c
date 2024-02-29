/*
 * Double-precision SVE 2^x function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

#define N (1 << V_EXP_TABLE_BITS)

#define BigBound 1022
#define UOFlowBound 1280

static const struct data
{
  double poly[4];
  double shift, big_bound, uoflow_bound;
} data = {
  /* Coefficients are computed using Remez algorithm with
     minimisation of the absolute error.  */
  .poly = { 0x1.62e42fefa3686p-1, 0x1.ebfbdff82c241p-3, 0x1.c6b09b16de99ap-5,
	    0x1.3b2abf5571ad8p-7 },
  .shift = 0x1.8p52 / N,
  .uoflow_bound = UOFlowBound,
  .big_bound = BigBound,
};

#define SpecialOffset 0x6000000000000000 /* 0x1p513.  */
/* SpecialBias1 + SpecialBias1 = asuint(1.0).  */
#define SpecialBias1 0x7000000000000000 /* 0x1p769.  */
#define SpecialBias2 0x3010000000000000 /* 0x1p-254.  */

/* Update of both special and non-special cases, if any special case is
   detected.  */
static inline svfloat64_t
special_case (svbool_t pg, svfloat64_t s, svfloat64_t y, svfloat64_t n,
	      const struct data *d)
{
  /* s=2^n may overflow, break it up into s=s1*s2,
     such that exp = s + s*y can be computed as s1*(s2+s2*y)
     and s1*s1 overflows only if n>0.  */

  /* If n<=0 then set b to 0x6, 0 otherwise.  */
  svbool_t p_sign = svcmple (pg, n, 0.0); /* n <= 0.  */
  svuint64_t b = svdup_u64_z (p_sign, SpecialOffset);

  /* Set s1 to generate overflow depending on sign of exponent n.  */
  svfloat64_t s1 = svreinterpret_f64 (svsubr_x (pg, b, SpecialBias1));
  /* Offset s to avoid overflow in final result if n is below threshold.  */
  svfloat64_t s2 = svreinterpret_f64 (
      svadd_x (pg, svsub_x (pg, svreinterpret_u64 (s), SpecialBias2), b));

  /* |n| > 1280 => 2^(n) overflows.  */
  svbool_t p_cmp = svacgt (pg, n, d->uoflow_bound);

  svfloat64_t r1 = svmul_x (pg, s1, s1);
  svfloat64_t r2 = svmla_x (pg, s2, s2, y);
  svfloat64_t r0 = svmul_x (pg, r2, s1);

  return svsel (p_cmp, r1, r0);
}

/* Fast vector implementation of exp2.
   Maximum measured error is 1.65 ulp.
   _ZGVsMxv_exp2(-0x1.4c264ab5b559bp-6) got 0x1.f8db0d4df721fp-1
				       want 0x1.f8db0d4df721dp-1.  */
svfloat64_t SV_NAME_D1 (exp2) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t no_big_scale = svacle (pg, x, d->big_bound);
  svbool_t special = svnot_z (pg, no_big_scale);

  /* Reduce x to k/N + r, where k is integer and r in [-1/2N, 1/2N].  */
  svfloat64_t shift = sv_f64 (d->shift);
  svfloat64_t kd = svadd_x (pg, x, shift);
  svuint64_t ki = svreinterpret_u64 (kd);
  /* kd = k/N.  */
  kd = svsub_x (pg, kd, shift);
  svfloat64_t r = svsub_x (pg, x, kd);

  /* scale ~= 2^(k/N).  */
  svuint64_t idx = svand_x (pg, ki, N - 1);
  svuint64_t sbits = svld1_gather_index (pg, __v_exp_data, idx);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  svuint64_t top = svlsl_x (pg, ki, 52 - V_EXP_TABLE_BITS);
  svfloat64_t scale = svreinterpret_f64 (svadd_x (pg, sbits, top));

  /* Approximate exp2(r) using polynomial.  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t p = sv_pairwise_poly_3_f64_x (pg, r, r2, d->poly);
  svfloat64_t y = svmul_x (pg, r, p);

  /* Assemble exp2(x) = exp2(r) * scale.  */
  if (unlikely (svptest_any (pg, special)))
    return special_case (pg, scale, y, kd, d);
  return svmla_x (pg, scale, scale, y);
}

PL_SIG (SV, D, 1, exp2, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_D1 (exp2), 1.15)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp2), 0, BigBound, 1000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp2), BigBound, UOFlowBound, 100000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp2), UOFlowBound, inf, 1000)
