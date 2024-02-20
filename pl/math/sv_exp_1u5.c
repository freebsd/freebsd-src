/*
 * Double-precision vector e^x function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  double poly[4];
  double ln2_hi, ln2_lo, inv_ln2, shift, thres;
} data = {
  .poly = { /* ulp error: 0.53.  */
	    0x1.fffffffffdbcdp-2, 0x1.555555555444cp-3, 0x1.555573c6a9f7dp-5,
	    0x1.1111266d28935p-7 },
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  /* 1/ln2.  */
  .inv_ln2 = 0x1.71547652b82fep+0,
  /* 1.5*2^46+1023. This value is further explained below.  */
  .shift = 0x1.800000000ffc0p+46,
  .thres = 704.0,
};

#define C(i) sv_f64 (d->poly[i])
#define SpecialOffset 0x6000000000000000 /* 0x1p513.  */
/* SpecialBias1 + SpecialBias1 = asuint(1.0).  */
#define SpecialBias1 0x7000000000000000 /* 0x1p769.  */
#define SpecialBias2 0x3010000000000000 /* 0x1p-254.  */

/* Update of both special and non-special cases, if any special case is
   detected.  */
static inline svfloat64_t
special_case (svbool_t pg, svfloat64_t s, svfloat64_t y, svfloat64_t n)
{
  /* s=2^n may overflow, break it up into s=s1*s2,
     such that exp = s + s*y can be computed as s1*(s2+s2*y)
     and s1*s1 overflows only if n>0.  */

  /* If n<=0 then set b to 0x6, 0 otherwise.  */
  svbool_t p_sign = svcmple (pg, n, 0.0); /* n <= 0.  */
  svuint64_t b
      = svdup_u64_z (p_sign, SpecialOffset); /* Inactive lanes set to 0.  */

  /* Set s1 to generate overflow depending on sign of exponent n.  */
  svfloat64_t s1 = svreinterpret_f64 (
      svsubr_x (pg, b, SpecialBias1)); /* 0x70...0 - b.  */
  /* Offset s to avoid overflow in final result if n is below threshold.  */
  svfloat64_t s2 = svreinterpret_f64 (
      svadd_x (pg, svsub_x (pg, svreinterpret_u64 (s), SpecialBias2),
	       b)); /* as_u64 (s) - 0x3010...0 + b.  */

  /* |n| > 1280 => 2^(n) overflows.  */
  svbool_t p_cmp = svacgt (pg, n, 1280.0);

  svfloat64_t r1 = svmul_x (pg, s1, s1);
  svfloat64_t r2 = svmla_x (pg, s2, s2, y);
  svfloat64_t r0 = svmul_x (pg, r2, s1);

  return svsel (p_cmp, r1, r0);
}

/* SVE exp algorithm. Maximum measured error is 1.01ulps:
   SV_NAME_D1 (exp)(0x1.4619d7b04da41p+6) got 0x1.885d9acc41da7p+117
					 want 0x1.885d9acc41da6p+117.  */
svfloat64_t SV_NAME_D1 (exp) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t special = svacgt (pg, x, d->thres);

  /* Use a modifed version of the shift used for flooring, such that x/ln2 is
     rounded to a multiple of 2^-6=1/64, shift = 1.5 * 2^52 * 2^-6 = 1.5 *
     2^46.

     n is not an integer but can be written as n = m + i/64, with i and m
     integer, 0 <= i < 64 and m <= n.

     Bits 5:0 of z will be null every time x/ln2 reaches a new integer value
     (n=m, i=0), and is incremented every time z (or n) is incremented by 1/64.
     FEXPA expects i in bits 5:0 of the input so it can be used as index into
     FEXPA hardwired table T[i] = 2^(i/64) for i = 0:63, that will in turn
     populate the mantissa of the output. Therefore, we use u=asuint(z) as
     input to FEXPA.

     We add 1023 to the modified shift value in order to set bits 16:6 of u to
     1, such that once these bits are moved to the exponent of the output of
     FEXPA, we get the exponent of 2^n right, i.e. we get 2^m.  */
  svfloat64_t z = svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2);
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t n = svsub_x (pg, z, d->shift);

  /* r = x - n * ln2, r is in [-ln2/(2N), ln2/(2N)].  */
  svfloat64_t ln2 = svld1rq (svptrue_b64 (), &d->ln2_hi);
  svfloat64_t r = svmls_lane (x, n, ln2, 0);
  r = svmls_lane (r, n, ln2, 1);

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5.  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t p01 = svmla_x (pg, C (0), C (1), r);
  svfloat64_t p23 = svmla_x (pg, C (2), C (3), r);
  svfloat64_t p04 = svmla_x (pg, p01, p23, r2);
  svfloat64_t y = svmla_x (pg, r, p04, r2);

  /* s = 2^n, computed using FEXPA. FEXPA does not propagate NaNs, so for
     consistent NaN handling we have to manually propagate them. This comes at
     significant performance cost.  */
  svfloat64_t s = svexpa (u);

  /* Assemble result as exp(x) = 2^n * exp(r).  If |x| > Thresh the
     multiplication may overflow, so use special case routine.  */

  if (unlikely (svptest_any (pg, special)))
    {
      /* FEXPA zeroes the sign bit, however the sign is meaningful to the
	 special case function so needs to be copied.
	 e = sign bit of u << 46.  */
      svuint64_t e = svand_x (pg, svlsl_x (pg, u, 46), 0x8000000000000000);
      /* Copy sign to s.  */
      s = svreinterpret_f64 (svadd_x (pg, e, svreinterpret_u64 (s)));
      return special_case (pg, s, y, n);
    }

  /* No special case.  */
  return svmla_x (pg, s, s, y);
}

PL_SIG (SV, D, 1, exp, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_D1 (exp), 1.46)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp), 0, 0x1p-23, 40000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp), 0x1p-23, 1, 50000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp), 1, 0x1p23, 50000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (exp), 0x1p23, inf, 50000)
