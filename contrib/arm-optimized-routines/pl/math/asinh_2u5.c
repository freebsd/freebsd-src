/*
 * Double-precision asinh(x) function
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "poly_scalar_f64.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffffffffffff
#define ExpM26 0x3e50000000000000 /* asuint64(0x1.0p-26).  */
#define One 0x3ff0000000000000	  /* asuint64(1.0).  */
#define Exp511 0x5fe0000000000000 /* asuint64(0x1.0p511).  */
#define Ln2 0x1.62e42fefa39efp-1

double
optr_aor_log_f64 (double);

/* Scalar double-precision asinh implementation. This routine uses different
   approaches on different intervals:

   |x| < 2^-26: Return x. Function is exact in this region.

   |x| < 1: Use custom order-17 polynomial. This is least accurate close to 1.
     The largest observed error in this region is 1.47 ULPs:
     asinh(0x1.fdfcd00cc1e6ap-1) got 0x1.c1d6bf874019bp-1
				want 0x1.c1d6bf874019cp-1.

   |x| < 2^511: Upper bound of this region is close to sqrt(DBL_MAX). Calculate
     the result directly using the definition asinh(x) = ln(x + sqrt(x*x + 1)).
     The largest observed error in this region is 2.03 ULPs:
     asinh(-0x1.00094e0f39574p+0) got -0x1.c3508eb6a681ep-1
				 want -0x1.c3508eb6a682p-1.

   |x| >= 2^511: We cannot square x without overflow at a low
     cost. At very large x, asinh(x) ~= ln(2x). At huge x we cannot
     even double x without overflow, so calculate this as ln(x) +
     ln(2). The largest observed error in this region is 0.98 ULPs at many
     values, for instance:
     asinh(0x1.5255a4cf10319p+975) got 0x1.52652f4cb26cbp+9
				  want 0x1.52652f4cb26ccp+9.  */
double
asinh (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t ia = ix & AbsMask;
  double ax = asdouble (ia);
  uint64_t sign = ix & ~AbsMask;

  if (ia < ExpM26)
    {
      return x;
    }

  if (ia < One)
    {
      double x2 = x * x;
      double z2 = x2 * x2;
      double z4 = z2 * z2;
      double z8 = z4 * z4;
      double p = estrin_17_f64 (x2, z2, z4, z8, z8 * z8, __asinh_data.poly);
      double y = fma (p, x2 * ax, ax);
      return asdouble (asuint64 (y) | sign);
    }

  if (unlikely (ia >= Exp511))
    {
      return asdouble (asuint64 (optr_aor_log_f64 (ax) + Ln2) | sign);
    }

  return asdouble (asuint64 (optr_aor_log_f64 (ax + sqrt (ax * ax + 1)))
		   | sign);
}

PL_SIG (S, D, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (asinh, 1.54)
PL_TEST_INTERVAL (asinh, -0x1p-26, 0x1p-26, 50000)
PL_TEST_INTERVAL (asinh, 0x1p-26, 1.0, 40000)
PL_TEST_INTERVAL (asinh, -0x1p-26, -1.0, 10000)
PL_TEST_INTERVAL (asinh, 1.0, 100.0, 40000)
PL_TEST_INTERVAL (asinh, -1.0, -100.0, 10000)
PL_TEST_INTERVAL (asinh, 100.0, inf, 50000)
PL_TEST_INTERVAL (asinh, -100.0, -inf, 10000)
