/*
 * Double-precision vector sincos function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Define _GNU_SOURCE in order to include sincos declaration. If building
   pre-GLIBC 2.1, or on a non-GNU conforming system, this routine will need to
   be linked against the scalar sincosf from math/.  */
#define _GNU_SOURCE
#include <math.h>
#undef _GNU_SOURCE

#include "sv_sincos_common.h"
#include "sv_math.h"
#include "pl_test.h"

static void NOINLINE
special_case (svfloat64_t x, svbool_t special, double *out_sin,
	      double *out_cos)
{
  svbool_t p = svptrue_pat_b64 (SV_VL1);
  for (int i = 0; i < svcntd (); i++)
    {
      if (svptest_any (special, p))
	sincos (svlastb (p, x), out_sin + i, out_cos + i);
      p = svpnext_b64 (svptrue_b64 (), p);
    }
}

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate polynomials.
   Largest observed error is for sin, 3.22 ULP:
   sv_sincos_sin (0x1.d70eef40f39b1p+12) got -0x1.ffe9537d5dbb7p-3
					want -0x1.ffe9537d5dbb4p-3.  */
void
_ZGVsMxvl8l8_sincos (svfloat64_t x, double *out_sin, double *out_cos,
		     svbool_t pg)
{
  const struct sv_sincos_data *d = ptr_barrier (&sv_sincos_data);
  svbool_t special = check_ge_rangeval (pg, x, d);

  svfloat64x2_t sc = sv_sincos_inline (pg, x, d);

  svst1 (pg, out_sin, svget2 (sc, 0));
  svst1 (pg, out_cos, svget2 (sc, 1));

  if (unlikely (svptest_any (pg, special)))
    special_case (x, special, out_sin, out_cos);
}

PL_TEST_ULP (_ZGVsMxv_sincos_sin, 2.73)
PL_TEST_ULP (_ZGVsMxv_sincos_cos, 2.73)
#define SV_SINCOS_INTERVAL(lo, hi, n)                                         \
  PL_TEST_INTERVAL (_ZGVsMxv_sincos_sin, lo, hi, n)                           \
  PL_TEST_INTERVAL (_ZGVsMxv_sincos_cos, lo, hi, n)
SV_SINCOS_INTERVAL (0, 0x1p23, 500000)
SV_SINCOS_INTERVAL (-0, -0x1p23, 500000)
SV_SINCOS_INTERVAL (0x1p23, inf, 10000)
SV_SINCOS_INTERVAL (-0x1p23, -inf, 10000)
