/*
 * Single-precision vector cexpi function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_sincosf_common.h"
#include "sv_math.h"
#include "pl_test.h"

static svfloat32x2_t NOINLINE
special_case (svfloat32_t x, svbool_t special, svfloat32x2_t y)
{
  return svcreate2 (sv_call_f32 (sinf, x, svget2 (y, 0), special),
		    sv_call_f32 (cosf, x, svget2 (y, 1), special));
}

/* Single-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate low-order
   polynomials.
   Worst-case error for sin is 1.67 ULP:
   v_cexpif_sin(0x1.c704c4p+19) got 0x1.fff698p-5 want 0x1.fff69cp-5
   Worst-case error for cos is 1.81 ULP:
   v_cexpif_cos(0x1.e506fp+19) got -0x1.ffec6ep-6 want -0x1.ffec72p-6.  */
svfloat32x2_t
_ZGVsMxv_cexpif (svfloat32_t x, svbool_t pg)
{
  const struct sv_sincosf_data *d = ptr_barrier (&sv_sincosf_data);
  svbool_t special = check_ge_rangeval (pg, x, d);

  svfloat32x2_t sc = sv_sincosf_inline (pg, x, d);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, special, sc);
  return sc;
}

PL_TEST_ULP (_ZGVsMxv_cexpif_sin, 1.17)
PL_TEST_ULP (_ZGVsMxv_cexpif_cos, 1.31)
#define SV_CEXPIF_INTERVAL(lo, hi, n)                                         \
  PL_TEST_INTERVAL (_ZGVsMxv_cexpif_sin, lo, hi, n)                           \
  PL_TEST_INTERVAL (_ZGVsMxv_cexpif_cos, lo, hi, n)
SV_CEXPIF_INTERVAL (0, 0x1p20, 500000)
SV_CEXPIF_INTERVAL (-0, -0x1p20, 500000)
SV_CEXPIF_INTERVAL (0x1p20, inf, 10000)
SV_CEXPIF_INTERVAL (-0x1p20, -inf, 10000)
