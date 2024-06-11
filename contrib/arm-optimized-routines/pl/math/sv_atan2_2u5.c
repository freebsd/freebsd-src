/*
 * Double-precision vector atan2(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f64.h"

static const struct data
{
  float64_t poly[20];
  float64_t pi_over_2;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-1022, 1.0].  */
  .poly = { -0x1.5555555555555p-2,  0x1.99999999996c1p-3, -0x1.2492492478f88p-3,
            0x1.c71c71bc3951cp-4,   -0x1.745d160a7e368p-4, 0x1.3b139b6a88ba1p-4,
            -0x1.11100ee084227p-4,  0x1.e1d0f9696f63bp-5, -0x1.aebfe7b418581p-5,
            0x1.842dbe9b0d916p-5,   -0x1.5d30140ae5e99p-5, 0x1.338e31eb2fbbcp-5,
            -0x1.00e6eece7de8p-5,   0x1.860897b29e5efp-6, -0x1.0051381722a59p-6,
            0x1.14e9dc19a4a4ep-7,  -0x1.d0062b42fe3bfp-9, 0x1.17739e210171ap-10,
            -0x1.ab24da7be7402p-13, 0x1.358851160a528p-16, },
  .pi_over_2 = 0x1.921fb54442d18p+0,
};

/* Useful constants.  */
#define SignMask sv_u64 (0x8000000000000000)

/* Special cases i.e. 0, infinity, nan (fall back to scalar calls).  */
static svfloat64_t NOINLINE
special_case (svfloat64_t y, svfloat64_t x, svfloat64_t ret,
	      const svbool_t cmp)
{
  return sv_call2_f64 (atan2, y, x, ret, cmp);
}

/* Returns a predicate indicating true if the input is the bit representation
   of 0, infinity or nan.  */
static inline svbool_t
zeroinfnan (svuint64_t i, const svbool_t pg)
{
  return svcmpge (pg, svsub_x (pg, svlsl_x (pg, i, 1), 1),
		  sv_u64 (2 * asuint64 (INFINITY) - 1));
}

/* Fast implementation of SVE atan2. Errors are greatest when y and
   x are reasonably close together. The greatest observed error is 2.28 ULP:
   _ZGVsMxvv_atan2 (-0x1.5915b1498e82fp+732, 0x1.54d11ef838826p+732)
   got -0x1.954f42f1fa841p-1 want -0x1.954f42f1fa843p-1.  */
svfloat64_t SV_NAME_D2 (atan2) (svfloat64_t y, svfloat64_t x, const svbool_t pg)
{
  const struct data *data_ptr = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t iy = svreinterpret_u64 (y);

  svbool_t cmp_x = zeroinfnan (ix, pg);
  svbool_t cmp_y = zeroinfnan (iy, pg);
  svbool_t cmp_xy = svorr_z (pg, cmp_x, cmp_y);

  svuint64_t sign_x = svand_x (pg, ix, SignMask);
  svuint64_t sign_y = svand_x (pg, iy, SignMask);
  svuint64_t sign_xy = sveor_x (pg, sign_x, sign_y);

  svfloat64_t ax = svabs_x (pg, x);
  svfloat64_t ay = svabs_x (pg, y);

  svbool_t pred_xlt0 = svcmplt (pg, x, 0.0);
  svbool_t pred_aygtax = svcmpgt (pg, ay, ax);

  /* Set up z for call to atan.  */
  svfloat64_t n = svsel (pred_aygtax, svneg_x (pg, ax), ay);
  svfloat64_t d = svsel (pred_aygtax, ay, ax);
  svfloat64_t z = svdiv_x (pg, n, d);

  /* Work out the correct shift.  */
  svfloat64_t shift = svsel (pred_xlt0, sv_f64 (-2.0), sv_f64 (0.0));
  shift = svsel (pred_aygtax, svadd_x (pg, shift, 1.0), shift);
  shift = svmul_x (pg, shift, data_ptr->pi_over_2);

  /* Use split Estrin scheme for P(z^2) with deg(P)=19.  */
  svfloat64_t z2 = svmul_x (pg, z, z);
  svfloat64_t x2 = svmul_x (pg, z2, z2);
  svfloat64_t x4 = svmul_x (pg, x2, x2);
  svfloat64_t x8 = svmul_x (pg, x4, x4);

  svfloat64_t ret = svmla_x (
      pg, sv_estrin_7_f64_x (pg, z2, x2, x4, data_ptr->poly),
      sv_estrin_11_f64_x (pg, z2, x2, x4, x8, data_ptr->poly + 8), x8);

  /* y = shift + z + z^3 * P(z^2).  */
  svfloat64_t z3 = svmul_x (pg, z2, z);
  ret = svmla_x (pg, z, z3, ret);

  ret = svadd_m (pg, ret, shift);

  /* Account for the sign of x and y.  */
  ret = svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ret), sign_xy));

  if (unlikely (svptest_any (pg, cmp_xy)))
    return special_case (y, x, ret, cmp_xy);

  return ret;
}

/* Arity of 2 means no mathbench entry emitted. See test/mathbench_funcs.h.  */
PL_SIG (SV, D, 2, atan2)
PL_TEST_ULP (SV_NAME_D2 (atan2), 1.78)
PL_TEST_INTERVAL (SV_NAME_D2 (atan2), 0.0, 1.0, 40000)
PL_TEST_INTERVAL (SV_NAME_D2 (atan2), 1.0, 100.0, 40000)
PL_TEST_INTERVAL (SV_NAME_D2 (atan2), 100, inf, 40000)
PL_TEST_INTERVAL (SV_NAME_D2 (atan2), -0, -inf, 40000)
