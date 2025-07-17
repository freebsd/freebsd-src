/*
 * Double-precision SVE asinh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define SignMask (0x8000000000000000)
#define One (0x3ff0000000000000)
#define Thres (0x5fe0000000000000) /* asuint64 (0x1p511).  */
#define IndexMask (((1 << V_LOG_TABLE_BITS) - 1) << 1)

static const struct data
{
  double even_coeffs[9];
  double ln2, p3, p1, p4, p0, p2, c1, c3, c5, c7, c9, c11, c13, c15, c17;
  uint64_t off, mask;

} data = {
   /* Polynomial generated using Remez on [2^-26, 1].  */
  .even_coeffs ={
    -0x1.55555555554a7p-3,
    -0x1.6db6db68332e6p-5,
    -0x1.6e8b8b654a621p-6,
    -0x1.c9871d10885afp-7,
    -0x1.3ddca533e9f54p-7,
    -0x1.b90c7099dd397p-8,
    -0x1.d217026a669ecp-9,
    -0x1.e0f37daef9127p-11,
    -0x1.021a48685e287p-14, },

  .c1 = 0x1.3333333326c7p-4,
  .c3 = 0x1.f1c71b26fb40dp-6,
  .c5 = 0x1.1c4daa9e67871p-6,
  .c7 = 0x1.7a16e8d9d2ecfp-7,
  .c9 = 0x1.0becef748dafcp-7,
  .c11 = 0x1.541f2bb1ffe51p-8,
  .c13 = 0x1.0b5c7977aaf7p-9,
  .c15 = 0x1.388b5fe542a6p-12,
  .c17 = 0x1.93d4ba83d34dap-18,

  .ln2 = 0x1.62e42fefa39efp-1,
  .p0 = -0x1.ffffffffffff7p-2,
  .p1 = 0x1.55555555170d4p-2,
  .p2 = -0x1.0000000399c27p-2,
  .p3 = 0x1.999b2e90e94cap-3,
  .p4 = -0x1.554e550bd501ep-3,
  .off = 0x3fe6900900000000,
  .mask = 0xfffULL << 52,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (asinh, x, y, special);
}

static inline svfloat64_t
__sv_log_inline (svfloat64_t x, const struct data *d, const svbool_t pg)
{
  /* Double-precision SVE log, copied from SVE log implementation with some
     cosmetic modification and special-cases removed. See that file for details
     of the algorithm used.  */

  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t i_off = svsub_x (pg, ix, d->off);
  svuint64_t i
      = svand_x (pg, svlsr_x (pg, i_off, (51 - V_LOG_TABLE_BITS)), IndexMask);
  svuint64_t iz = svsub_x (pg, ix, svand_x (pg, i_off, d->mask));
  svfloat64_t z = svreinterpret_f64 (iz);

  svfloat64_t invc = svld1_gather_index (pg, &__v_log_data.table[0].invc, i);
  svfloat64_t logc = svld1_gather_index (pg, &__v_log_data.table[0].logc, i);

  svfloat64_t ln2_p3 = svld1rq (svptrue_b64 (), &d->ln2);
  svfloat64_t p1_p4 = svld1rq (svptrue_b64 (), &d->p1);

  svfloat64_t r = svmla_x (pg, sv_f64 (-1.0), invc, z);
  svfloat64_t kd
      = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (i_off), 52));

  svfloat64_t hi = svmla_lane (svadd_x (pg, logc, r), kd, ln2_p3, 0);
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t y = svmla_lane (sv_f64 (d->p2), r, ln2_p3, 1);
  svfloat64_t p = svmla_lane (sv_f64 (d->p0), r, p1_p4, 0);

  y = svmla_lane (y, r2, p1_p4, 1);
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
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t iax = svbic_x (pg, ix, SignMask);
  svuint64_t sign = svand_x (pg, ix, SignMask);
  svfloat64_t ax = svreinterpret_f64 (iax);
  svbool_t ge1 = svcmpge (pg, iax, One);
  svbool_t special = svcmpge (pg, iax, Thres);

  /* Option 1: |x| >= 1.
     Compute asinh(x) according by asinh(x) = log(x + sqrt(x^2 + 1)).  */
  svfloat64_t option_1 = sv_f64 (0);
  if (likely (svptest_any (pg, ge1)))
    {
      svfloat64_t x2 = svmul_x (svptrue_b64 (), ax, ax);
      option_1 = __sv_log_inline (
	  svadd_x (pg, ax, svsqrt_x (pg, svadd_x (pg, x2, 1))), d, pg);
    }

  /* Option 2: |x| < 1.
     Compute asinh(x) using a polynomial.
     The largest observed error in this region is 1.51 ULPs:
     _ZGVsMxv_asinh(0x1.fe12bf8c616a2p-1) got 0x1.c1e649ee2681bp-1
					 want 0x1.c1e649ee2681dp-1.  */

  svfloat64_t option_2 = sv_f64 (0);
  if (likely (svptest_any (pg, svnot_z (pg, ge1))))
    {
      svfloat64_t x2 = svmul_x (svptrue_b64 (), ax, ax);
      svfloat64_t x4 = svmul_x (svptrue_b64 (), x2, x2);
      /* Order-17 Pairwise Horner scheme.  */
      svfloat64_t c13 = svld1rq (svptrue_b64 (), &d->c1);
      svfloat64_t c57 = svld1rq (svptrue_b64 (), &d->c5);
      svfloat64_t c911 = svld1rq (svptrue_b64 (), &d->c9);
      svfloat64_t c1315 = svld1rq (svptrue_b64 (), &d->c13);

      svfloat64_t p01 = svmla_lane (sv_f64 (d->even_coeffs[0]), x2, c13, 0);
      svfloat64_t p23 = svmla_lane (sv_f64 (d->even_coeffs[1]), x2, c13, 1);
      svfloat64_t p45 = svmla_lane (sv_f64 (d->even_coeffs[2]), x2, c57, 0);
      svfloat64_t p67 = svmla_lane (sv_f64 (d->even_coeffs[3]), x2, c57, 1);
      svfloat64_t p89 = svmla_lane (sv_f64 (d->even_coeffs[4]), x2, c911, 0);
      svfloat64_t p1011 = svmla_lane (sv_f64 (d->even_coeffs[5]), x2, c911, 1);
      svfloat64_t p1213
	  = svmla_lane (sv_f64 (d->even_coeffs[6]), x2, c1315, 0);
      svfloat64_t p1415
	  = svmla_lane (sv_f64 (d->even_coeffs[7]), x2, c1315, 1);
      svfloat64_t p1617 = svmla_x (pg, sv_f64 (d->even_coeffs[8]), x2, d->c17);

      svfloat64_t p = svmla_x (pg, p1415, x4, p1617);
      p = svmla_x (pg, p1213, x4, p);
      p = svmla_x (pg, p1011, x4, p);
      p = svmla_x (pg, p89, x4, p);

      p = svmla_x (pg, p67, x4, p);
      p = svmla_x (pg, p45, x4, p);

      p = svmla_x (pg, p23, x4, p);

      p = svmla_x (pg, p01, x4, p);

      option_2 = svmla_x (pg, ax, p, svmul_x (svptrue_b64 (), x2, ax));
    }

  if (unlikely (svptest_any (pg, special)))
    return special_case (
	x,
	svreinterpret_f64 (sveor_x (
	    pg, svreinterpret_u64 (svsel (ge1, option_1, option_2)), sign)),
	special);

  /* Choose the right option for each lane.  */
  svfloat64_t y = svsel (ge1, option_1, option_2);
  return svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (y), sign));
}

TEST_SIG (SV, D, 1, asinh, -10.0, 10.0)
TEST_ULP (SV_NAME_D1 (asinh), 2.52)
TEST_DISABLE_FENV (SV_NAME_D1 (asinh))
TEST_SYM_INTERVAL (SV_NAME_D1 (asinh), 0, 0x1p-26, 50000)
TEST_SYM_INTERVAL (SV_NAME_D1 (asinh), 0x1p-26, 1, 50000)
TEST_SYM_INTERVAL (SV_NAME_D1 (asinh), 1, 0x1p511, 50000)
TEST_SYM_INTERVAL (SV_NAME_D1 (asinh), 0x1p511, inf, 40000)
/* Test vector asinh 3 times, with control lane < 1, > 1 and special.
   Ensures the v_sel is choosing the right option in all cases.  */
TEST_CONTROL_VALUE (SV_NAME_D1 (asinh), 0.5)
TEST_CONTROL_VALUE (SV_NAME_D1 (asinh), 2)
TEST_CONTROL_VALUE (SV_NAME_D1 (asinh), 0x1p600)
CLOSE_SVE_ATTR
