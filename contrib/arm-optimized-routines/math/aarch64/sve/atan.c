/*
 * Double-precision vector atan(x) function.
 *
 * Copyright (c) 2021-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_poly_f64.h"

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
#define SignMask (0x8000000000000000)

/* Fast implementation of SVE atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=1/x and shift = pi/2. Largest errors are close to 1. The maximum observed
   error is 2.27 ulps:
   _ZGVsMxv_atan (0x1.0005af27c23e9p+0) got 0x1.9225645bdd7c1p-1
				       want 0x1.9225645bdd7c3p-1.  */
svfloat64_t SV_NAME_D1 (atan) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* No need to trigger special case. Small cases, infs and nans
     are supported by our approximation technique.  */
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t sign = svand_x (pg, ix, SignMask);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  svbool_t red = svacgt (pg, x, 1.0);
  /* Avoid dependency in abs(x) in division (and comparison).  */
  svfloat64_t z = svsel (red, svdivr_x (pg, x, 1.0), x);
  /* Use absolute value only when needed (odd powers of z).  */
  svfloat64_t az = svabs_x (pg, z);
  az = svneg_m (az, red, az);

  /* Use split Estrin scheme for P(z^2) with deg(P)=19.  */
  svfloat64_t z2 = svmul_x (pg, z, z);
  svfloat64_t x2 = svmul_x (pg, z2, z2);
  svfloat64_t x4 = svmul_x (pg, x2, x2);
  svfloat64_t x8 = svmul_x (pg, x4, x4);

  svfloat64_t y
      = svmla_x (pg, sv_estrin_7_f64_x (pg, z2, x2, x4, d->poly),
		 sv_estrin_11_f64_x (pg, z2, x2, x4, x8, d->poly + 8), x8);

  /* y = shift + z + z^3 * P(z^2).  */
  svfloat64_t z3 = svmul_x (pg, z2, az);
  y = svmla_x (pg, az, z3, y);

  /* Apply shift as indicated by `red` predicate.  */
  y = svadd_m (red, y, d->pi_over_2);

  /* y = atan(x) if x>0, -atan(-x) otherwise.  */
  y = svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (y), sign));

  return y;
}

TEST_SIG (SV, D, 1, atan, -3.1, 3.1)
TEST_ULP (SV_NAME_D1 (atan), 1.78)
TEST_DISABLE_FENV (SV_NAME_D1 (atan))
TEST_INTERVAL (SV_NAME_D1 (atan), 0.0, 1.0, 40000)
TEST_INTERVAL (SV_NAME_D1 (atan), 1.0, 100.0, 40000)
TEST_INTERVAL (SV_NAME_D1 (atan), 100, inf, 40000)
TEST_INTERVAL (SV_NAME_D1 (atan), -0, -inf, 40000)
CLOSE_SVE_ATTR
