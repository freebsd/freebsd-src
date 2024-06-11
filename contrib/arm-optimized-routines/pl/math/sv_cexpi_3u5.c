/*
 * Double-precision vector cexpi function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_sincos_common.h"
#include "sv_math.h"
#include "pl_test.h"

static svfloat64x2_t NOINLINE
special_case (svfloat64_t x, svbool_t special, svfloat64x2_t y)
{
  return svcreate2 (sv_call_f64 (sin, x, svget2 (y, 0), special),
		    sv_call_f64 (cos, x, svget2 (y, 1), special));
}

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate polynomials.
   Largest observed error is for sin, 3.22 ULP:
   sv_cexpi_sin (0x1.d70eef40f39b1p+12) got -0x1.ffe9537d5dbb7p-3
				       want -0x1.ffe9537d5dbb4p-3.  */
svfloat64x2_t
_ZGVsMxv_cexpi (svfloat64_t x, svbool_t pg)
{
  const struct sv_sincos_data *d = ptr_barrier (&sv_sincos_data);
  svbool_t special = check_ge_rangeval (pg, x, d);

  svfloat64x2_t sc = sv_sincos_inline (pg, x, d);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, special, sc);
  return sc;
}

PL_TEST_ULP (_ZGVsMxv_cexpi_sin, 2.73)
PL_TEST_ULP (_ZGVsMxv_cexpi_cos, 2.73)
#define SV_CEXPI_INTERVAL(lo, hi, n)                                          \
  PL_TEST_INTERVAL (_ZGVsMxv_cexpi_sin, lo, hi, n)                            \
  PL_TEST_INTERVAL (_ZGVsMxv_cexpi_cos, lo, hi, n)
SV_CEXPI_INTERVAL (0, 0x1p23, 500000)
SV_CEXPI_INTERVAL (-0, -0x1p23, 500000)
SV_CEXPI_INTERVAL (0x1p23, inf, 10000)
SV_CEXPI_INTERVAL (-0x1p23, -inf, 10000)
