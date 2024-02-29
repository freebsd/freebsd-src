/*
 * Single-precision vector sincos function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Define _GNU_SOURCE in order to include sincosf declaration. If building
   pre-GLIBC 2.1, or on a non-GNU conforming system, this routine will need to
   be linked against the scalar sincosf from math/.  */
#define _GNU_SOURCE
#include <math.h>
#undef _GNU_SOURCE

#include "sv_sincosf_common.h"
#include "sv_math.h"
#include "pl_test.h"

static void NOINLINE
special_case (svfloat32_t x, svbool_t special, float *out_sin, float *out_cos)
{
  svbool_t p = svptrue_pat_b32 (SV_VL1);
  for (int i = 0; i < svcntw (); i++)
    {
      if (svptest_any (special, p))
	sincosf (svlastb (p, x), out_sin + i, out_cos + i);
      p = svpnext_b32 (svptrue_b32 (), p);
    }
}

/* Single-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate low-order
   polynomials.
   Worst-case error for sin is 1.67 ULP:
   sv_sincosf_sin(0x1.c704c4p+19) got 0x1.fff698p-5 want 0x1.fff69cp-5
   Worst-case error for cos is 1.81 ULP:
   sv_sincosf_cos(0x1.e506fp+19) got -0x1.ffec6ep-6 want -0x1.ffec72p-6.  */
void
_ZGVsMxvl4l4_sincosf (svfloat32_t x, float *out_sin, float *out_cos,
		      svbool_t pg)
{
  const struct sv_sincosf_data *d = ptr_barrier (&sv_sincosf_data);
  svbool_t special = check_ge_rangeval (pg, x, d);

  svfloat32x2_t sc = sv_sincosf_inline (pg, x, d);

  svst1_f32 (pg, out_sin, svget2 (sc, 0));
  svst1_f32 (pg, out_cos, svget2 (sc, 1));

  if (unlikely (svptest_any (pg, special)))
    special_case (x, special, out_sin, out_cos);
}

PL_TEST_ULP (_ZGVsMxv_sincosf_sin, 1.17)
PL_TEST_ULP (_ZGVsMxv_sincosf_cos, 1.31)
#define SV_SINCOSF_INTERVAL(lo, hi, n)                                        \
  PL_TEST_INTERVAL (_ZGVsMxv_sincosf_sin, lo, hi, n)                          \
  PL_TEST_INTERVAL (_ZGVsMxv_sincosf_cos, lo, hi, n)
SV_SINCOSF_INTERVAL (0, 0x1p20, 500000)
SV_SINCOSF_INTERVAL (-0, -0x1p20, 500000)
SV_SINCOSF_INTERVAL (0x1p20, inf, 10000)
SV_SINCOSF_INTERVAL (-0x1p20, -inf, 10000)
