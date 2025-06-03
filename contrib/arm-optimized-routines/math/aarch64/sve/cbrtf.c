/*
 * Single-precision SVE cbrt(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_poly_f32.h"

const static struct data
{
  float32_t poly[4];
  float32_t table[5];
  float32_t one_third, two_thirds;
} data = {
  /* Very rough approximation of cbrt(x) in [0.5, 1], generated with FPMinimax.
   */
  .poly = { 0x1.c14e96p-2, 0x1.dd2d3p-1, -0x1.08e81ap-1,
	    0x1.2c74c2p-3, },
  /* table[i] = 2^((i - 2) / 3).  */
  .table = { 0x1.428a3p-1, 0x1.965feap-1, 0x1p0, 0x1.428a3p0, 0x1.965feap0 },
  .one_third = 0x1.555556p-2f,
  .two_thirds = 0x1.555556p-1f,
};

#define SmallestNormal 0x00800000
#define Thresh 0x7f000000 /* asuint(INFINITY) - SmallestNormal.  */
#define MantissaMask 0x007fffff
#define HalfExp 0x3f000000

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (cbrtf, x, y, special);
}

static inline svfloat32_t
shifted_lookup (const svbool_t pg, const float32_t *table, svint32_t i)
{
  return svld1_gather_index (pg, table, svadd_x (pg, i, 2));
}

/* Approximation for vector single-precision cbrt(x) using Newton iteration
   with initial guess obtained by a low-order polynomial. Greatest error
   is 1.64 ULP. This is observed for every value where the mantissa is
   0x1.85a2aa and the exponent is a multiple of 3, for example:
   _ZGVsMxv_cbrtf (0x1.85a2aap+3) got 0x1.267936p+1
				 want 0x1.267932p+1.  */
svfloat32_t SV_NAME_F1 (cbrt) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);

  /* Subnormal, +/-0 and special values.  */
  svbool_t special = svcmpge (pg, svsub_x (pg, iax, SmallestNormal), Thresh);

  /* Decompose |x| into m * 2^e, where m is in [0.5, 1.0]. This is a vector
     version of frexpf, which gets subnormal values wrong - these have to be
     special-cased as a result.  */
  svfloat32_t m = svreinterpret_f32 (svorr_x (
      pg, svand_x (pg, svreinterpret_u32 (x), MantissaMask), HalfExp));
  svint32_t e = svsub_x (pg, svreinterpret_s32 (svlsr_x (pg, iax, 23)), 126);

  /* p is a rough approximation for cbrt(m) in [0.5, 1.0]. The better this is,
     the less accurate the next stage of the algorithm needs to be. An order-4
     polynomial is enough for one Newton iteration.  */
  svfloat32_t p
      = sv_pairwise_poly_3_f32_x (pg, m, svmul_x (pg, m, m), d->poly);

  /* One iteration of Newton's method for iteratively approximating cbrt.  */
  svfloat32_t m_by_3 = svmul_x (pg, m, d->one_third);
  svfloat32_t a = svmla_x (pg, svdiv_x (pg, m_by_3, svmul_x (pg, p, p)), p,
			   d->two_thirds);

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     We can get 2 ^ round(e / 3) using ldexp and integer divide, but since e is
     not necessarily a multiple of 3 we lose some information.

     Let q = 2 ^ round(e / 3), then t = 2 ^ (e / 3) / q.

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3, which
     is an integer in [-2, 2], and can be looked up in the table T. Hence the
     result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.  */
  svfloat32_t ef = svmul_x (pg, svcvt_f32_x (pg, e), d->one_third);
  svint32_t ey = svcvt_s32_x (pg, ef);
  svint32_t em3 = svmls_x (pg, e, ey, 3);

  svfloat32_t my = shifted_lookup (pg, d->table, em3);
  my = svmul_x (pg, my, a);

  /* Vector version of ldexpf.  */
  svfloat32_t y = svscale_x (pg, my, ey);

  if (unlikely (svptest_any (pg, special)))
    return special_case (
	x, svreinterpret_f32 (svorr_x (pg, svreinterpret_u32 (y), sign)),
	special);

  /* Copy sign.  */
  return svreinterpret_f32 (svorr_x (pg, svreinterpret_u32 (y), sign));
}

TEST_SIG (SV, F, 1, cbrt, -10.0, 10.0)
TEST_ULP (SV_NAME_F1 (cbrt), 1.15)
TEST_DISABLE_FENV (SV_NAME_F1 (cbrt))
TEST_SYM_INTERVAL (SV_NAME_F1 (cbrt), 0, inf, 1000000)
CLOSE_SVE_ATTR
