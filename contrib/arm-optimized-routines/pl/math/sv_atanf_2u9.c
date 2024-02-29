/*
 * Single-precision vector atan(x) function.
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

#define SignMask (0x80000000)

/* Fast implementation of SVE atanf based on
   atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=-1/x and shift = pi/2.
   Largest observed error is 2.9 ULP, close to +/-1.0:
   _ZGVsMxv_atanf (0x1.0468f6p+0) got -0x1.967f06p-1
				 want -0x1.967fp-1.  */
svfloat32_t SV_NAME_F1 (atan) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* No need to trigger special case. Small cases, infs and nans
     are supported by our approximation technique.  */
  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t sign = svand_x (pg, ix, SignMask);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  svbool_t red = svacgt (pg, x, 1.0f);
  /* Avoid dependency in abs(x) in division (and comparison).  */
  svfloat32_t z = svsel (red, svdiv_x (pg, sv_f32 (1.0f), x), x);
  /* Use absolute value only when needed (odd powers of z).  */
  svfloat32_t az = svabs_x (pg, z);
  az = svneg_m (az, red, az);

  /* Use split Estrin scheme for P(z^2) with deg(P)=7.  */
  svfloat32_t z2 = svmul_x (pg, z, z);
  svfloat32_t z4 = svmul_x (pg, z2, z2);
  svfloat32_t z8 = svmul_x (pg, z4, z4);

  svfloat32_t y = sv_estrin_7_f32_x (pg, z2, z4, z8, d->poly);

  /* y = shift + z + z^3 * P(z^2).  */
  svfloat32_t z3 = svmul_x (pg, z2, az);
  y = svmla_x (pg, az, z3, y);

  /* Apply shift as indicated by 'red' predicate.  */
  y = svadd_m (red, y, sv_f32 (d->pi_over_2));

  /* y = atan(x) if x>0, -atan(-x) otherwise.  */
  return svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (y), sign));
}

PL_SIG (SV, F, 1, atan, -3.1, 3.1)
PL_TEST_ULP (SV_NAME_F1 (atan), 2.9)
PL_TEST_INTERVAL (SV_NAME_F1 (atan), 0.0, 1.0, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (atan), 1.0, 100.0, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (atan), 100, inf, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (atan), -0, -inf, 40000)
