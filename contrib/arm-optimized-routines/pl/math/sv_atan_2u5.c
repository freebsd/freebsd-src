/*
 * Double-precision vector atan(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#include "sv_atan_common.h"

/* Useful constants.  */
#define PiOver2 sv_f64 (0x1.921fb54442d18p+0)
#define AbsMask (0x7fffffffffffffff)

/* Fast implementation of SVE atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=1/x and shift = pi/2. Largest errors are close to 1. The maximum observed
   error is 2.27 ulps:
   __sv_atan(0x1.0005af27c23e9p+0) got 0x1.9225645bdd7c1p-1
				  want 0x1.9225645bdd7c3p-1.  */
sv_f64_t
__sv_atan_x (sv_f64_t x, const svbool_t pg)
{
  /* No need to trigger special case. Small cases, infs and nans
     are supported by our approximation technique.  */
  sv_u64_t ix = sv_as_u64_f64 (x);
  sv_u64_t sign = svand_n_u64_x (pg, ix, ~AbsMask);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  svbool_t red = svacgt_n_f64 (pg, x, 1.0);
  /* Avoid dependency in abs(x) in division (and comparison).  */
  sv_f64_t z = svsel_f64 (red, svdiv_f64_x (pg, sv_f64 (-1.0), x), x);
  /* Use absolute value only when needed (odd powers of z).  */
  sv_f64_t az = svabs_f64_x (pg, z);
  az = svneg_f64_m (az, red, az);

  sv_f64_t y = __sv_atan_common (pg, red, z, az, PiOver2);

  /* y = atan(x) if x>0, -atan(-x) otherwise.  */
  y = sv_as_f64_u64 (sveor_u64_x (pg, sv_as_u64_f64 (y), sign));

  return y;
}

PL_ALIAS (__sv_atan_x, _ZGVsMxv_atan)

PL_SIG (SV, D, 1, atan, -3.1, 3.1)
PL_TEST_ULP (__sv_atan, 1.78)
PL_TEST_INTERVAL (__sv_atan, -10.0, 10.0, 50000)
PL_TEST_INTERVAL (__sv_atan, -1.0, 1.0, 40000)
PL_TEST_INTERVAL (__sv_atan, 0.0, 1.0, 40000)
PL_TEST_INTERVAL (__sv_atan, 1.0, 100.0, 40000)
PL_TEST_INTERVAL (__sv_atan, 1e6, 1e32, 40000)
#endif
