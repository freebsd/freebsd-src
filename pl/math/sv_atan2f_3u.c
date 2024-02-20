/*
 * Single-precision vector atan2f(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f32.h"

static const struct data
{
  float32_t poly[8];
  float32_t pi_over_2;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].  */
  .poly = { -0x1.55555p-2f, 0x1.99935ep-3f, -0x1.24051ep-3f, 0x1.bd7368p-4f,
	    -0x1.491f0ep-4f, 0x1.93a2c0p-5f, -0x1.4c3c60p-6f, 0x1.01fd88p-8f },
  .pi_over_2 = 0x1.921fb6p+0f,
};

#define SignMask sv_u32 (0x80000000)

/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
static inline svfloat32_t
special_case (svfloat32_t y, svfloat32_t x, svfloat32_t ret,
	      const svbool_t cmp)
{
  return sv_call2_f32 (atan2f, y, x, ret, cmp);
}

/* Returns a predicate indicating true if the input is the bit representation
   of 0, infinity or nan.  */
static inline svbool_t
zeroinfnan (svuint32_t i, const svbool_t pg)
{
  return svcmpge (pg, svsub_x (pg, svlsl_x (pg, i, 1), 1),
		  sv_u32 (2 * 0x7f800000lu - 1));
}

/* Fast implementation of SVE atan2f based on atan(x) ~ shift + z + z^3 *
   P(z^2) with reduction to [0,1] using z=1/x and shift = pi/2. Maximum
   observed error is 2.95 ULP:
   _ZGVsMxvv_atan2f (0x1.93836cp+6, 0x1.8cae1p+6) got 0x1.967f06p-1
						 want 0x1.967f00p-1.  */
svfloat32_t SV_NAME_F2 (atan2) (svfloat32_t y, svfloat32_t x, const svbool_t pg)
{
  const struct data *data_ptr = ptr_barrier (&data);

  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t iy = svreinterpret_u32 (y);

  svbool_t cmp_x = zeroinfnan (ix, pg);
  svbool_t cmp_y = zeroinfnan (iy, pg);
  svbool_t cmp_xy = svorr_z (pg, cmp_x, cmp_y);

  svuint32_t sign_x = svand_x (pg, ix, SignMask);
  svuint32_t sign_y = svand_x (pg, iy, SignMask);
  svuint32_t sign_xy = sveor_x (pg, sign_x, sign_y);

  svfloat32_t ax = svabs_x (pg, x);
  svfloat32_t ay = svabs_x (pg, y);

  svbool_t pred_xlt0 = svcmplt (pg, x, 0.0);
  svbool_t pred_aygtax = svcmpgt (pg, ay, ax);

  /* Set up z for call to atan.  */
  svfloat32_t n = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat32_t d = svsel (pred_aygtax, ay, ax);
  svfloat32_t z = svdiv_x (pg, n, d);

  /* Work out the correct shift.  */
  svfloat32_t shift = svsel (pred_xlt0, sv_f32 (-2.0), sv_f32 (0.0));
  shift = svsel (pred_aygtax, svadd_x (pg, shift, 1.0), shift);
  shift = svmul_x (pg, shift, sv_f32 (data_ptr->pi_over_2));

  /* Use split Estrin scheme for P(z^2) with deg(P)=7.  */
  svfloat32_t z2 = svmul_x (pg, z, z);
  svfloat32_t z4 = svmul_x (pg, z2, z2);
  svfloat32_t z8 = svmul_x (pg, z4, z4);

  svfloat32_t ret = sv_estrin_7_f32_x (pg, z2, z4, z8, data_ptr->poly);

  /* ret = shift + z + z^3 * P(z^2).  */
  svfloat32_t z3 = svmul_x (pg, z2, z);
  ret = svmla_x (pg, z, z3, ret);

  ret = svadd_m (pg, ret, shift);

  /* Account for the sign of x and y.  */
  ret = svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (ret), sign_xy));

  if (unlikely (svptest_any (pg, cmp_xy)))
    return special_case (y, x, ret, cmp_xy);

  return ret;
}

/* Arity of 2 means no mathbench entry emitted. See test/mathbench_funcs.h.  */
PL_SIG (SV, F, 2, atan2)
PL_TEST_ULP (SV_NAME_F2 (atan2), 2.45)
PL_TEST_INTERVAL (SV_NAME_F2 (atan2), 0.0, 1.0, 40000)
PL_TEST_INTERVAL (SV_NAME_F2 (atan2), 1.0, 100.0, 40000)
PL_TEST_INTERVAL (SV_NAME_F2 (atan2), 100, inf, 40000)
PL_TEST_INTERVAL (SV_NAME_F2 (atan2), -0, -inf, 40000)
