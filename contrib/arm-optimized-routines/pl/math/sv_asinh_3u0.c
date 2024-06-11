/*
 * Double-precision SVE asinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

#define OneTop sv_u64 (0x3ff)	 /* top12(asuint64(1.0f)).  */
#define HugeBound sv_u64 (0x5fe) /* top12(asuint64(0x1p511)).  */
#define TinyBound (0x3e5)	 /* top12(asuint64(0x1p-26)).  */
#define SignMask (0x8000000000000000)

/* Constants & data for log.  */
#define A(i) __v_log_data.poly[i]
#define Ln2 (0x1.62e42fefa39efp-1)
#define N (1 << V_LOG_TABLE_BITS)
#define OFF (0x3fe6900900000000)

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (asinh, x, y, special);
}

static inline svfloat64_t
__sv_log_inline (svfloat64_t x, const svbool_t pg)
{
  /* Double-precision SVE log, copied from pl/math/sv_log_2u5.c with some
     cosmetic modification and special-cases removed. See that file for details
     of the algorithm used.  */
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t tmp = svsub_x (pg, ix, OFF);
  svuint64_t i
      = svand_x (pg, svlsr_x (pg, tmp, (51 - V_LOG_TABLE_BITS)), (N - 1) << 1);
  svint64_t k = svasr_x (pg, svreinterpret_s64 (tmp), 52);
  svuint64_t iz = svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52));
  svfloat64_t z = svreinterpret_f64 (iz);
  svfloat64_t invc = svld1_gather_index (pg, &__v_log_data.table[0].invc, i);
  svfloat64_t logc = svld1_gather_index (pg, &__v_log_data.table[0].logc, i);
  svfloat64_t r = svmla_x (pg, sv_f64 (-1.0), invc, z);
  svfloat64_t kd = svcvt_f64_x (pg, k);
  svfloat64_t hi = svmla_x (pg, svadd_x (pg, logc, r), kd, Ln2);
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t y = svmla_x (pg, sv_f64 (A (2)), r, A (3));
  svfloat64_t p = svmla_x (pg, sv_f64 (A (0)), r, A (1));
  y = svmla_x (pg, y, r2, A (4));
  y = svmla_x (pg, p, r2, y);
  y = svmla_x (pg, hi, r2, y);
  return y;
}

/* Double-precision implementation of SVE asinh(x).
   asinh is very sensitive around 1, so it is impractical to devise a single
   low-cost algorithm which is sufficiently accurate on a wide range of input.
   Instead we use two different algorithms:
   asinh(x) = sign(x) * log(|x| + sqrt(x^2 + 1)      if |x| >= 1
	    = sign(x) * (|x| + |x|^3 * P(x^2))       otherwise
   where log(x) is an optimized log approximation, and P(x) is a polynomial
   shared with the scalar routine. The greatest observed error 2.51 ULP, in
   |x| >= 1:
   _ZGVsMxv_asinh(0x1.170469d024505p+0) got 0x1.e3181c43b0f36p-1
				       want 0x1.e3181c43b0f39p-1.  */
svfloat64_t SV_NAME_D1 (asinh) (svfloat64_t x, const svbool_t pg)
{
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t iax = svbic_x (pg, ix, SignMask);
  svuint64_t sign = svand_x (pg, ix, SignMask);
  svfloat64_t ax = svreinterpret_f64 (iax);
  svuint64_t top12 = svlsr_x (pg, iax, 52);

  svbool_t ge1 = svcmpge (pg, top12, OneTop);
  svbool_t special = svcmpge (pg, top12, HugeBound);

  /* Option 1: |x| >= 1.
     Compute asinh(x) according by asinh(x) = log(x + sqrt(x^2 + 1)).  */
  svfloat64_t option_1 = sv_f64 (0);
  if (likely (svptest_any (pg, ge1)))
    {
      svfloat64_t axax = svmul_x (pg, ax, ax);
      option_1 = __sv_log_inline (
	  svadd_x (pg, ax, svsqrt_x (pg, svadd_x (pg, axax, 1))), pg);
    }

  /* Option 2: |x| < 1.
     Compute asinh(x) using a polynomial.
     The largest observed error in this region is 1.51 ULPs:
     _ZGVsMxv_asinh(0x1.fe12bf8c616a2p-1) got 0x1.c1e649ee2681bp-1
					 want 0x1.c1e649ee2681dp-1.  */
  svfloat64_t option_2 = sv_f64 (0);
  if (likely (svptest_any (pg, svnot_z (pg, ge1))))
    {
      svfloat64_t x2 = svmul_x (pg, ax, ax);
      svfloat64_t z2 = svmul_x (pg, x2, x2);
      svfloat64_t z4 = svmul_x (pg, z2, z2);
      svfloat64_t z8 = svmul_x (pg, z4, z4);
      svfloat64_t z16 = svmul_x (pg, z8, z8);
      svfloat64_t p
	  = sv_estrin_17_f64_x (pg, x2, z2, z4, z8, z16, __asinh_data.poly);
      option_2 = svmla_x (pg, ax, p, svmul_x (pg, x2, ax));
    }

  /* Choose the right option for each lane.  */
  svfloat64_t y = svsel (ge1, option_1, option_2);

  /* Apply sign of x to y.  */
  y = svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (y), sign));

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, y, special);
  return y;
}

PL_SIG (SV, D, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_D1 (asinh), 2.52)
/* Test vector asinh 3 times, with control lane < 1, > 1 and special.
   Ensures the svsel is choosing the right option in all cases.  */
#define SV_ASINH_INTERVAL(lo, hi, n)                                          \
  PL_TEST_SYM_INTERVAL_C (SV_NAME_D1 (asinh), lo, hi, n, 0.5)                 \
  PL_TEST_SYM_INTERVAL_C (SV_NAME_D1 (asinh), lo, hi, n, 2)                   \
  PL_TEST_SYM_INTERVAL_C (SV_NAME_D1 (asinh), lo, hi, n, 0x1p600)
SV_ASINH_INTERVAL (0, 0x1p-26, 50000)
SV_ASINH_INTERVAL (0x1p-26, 1, 50000)
SV_ASINH_INTERVAL (1, 0x1p511, 50000)
SV_ASINH_INTERVAL (0x1p511, inf, 40000)
