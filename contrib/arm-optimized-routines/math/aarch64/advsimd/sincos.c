/*
 * Double-precision vector sincos function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Define _GNU_SOURCE in order to include sincos declaration. If building
   pre-GLIBC 2.1, or on a non-GNU conforming system, this routine will need to
   be linked against the scalar sincosf from math/.  */
#define _GNU_SOURCE
#include <math.h>

#include "v_math.h"
#include "test_defs.h"
#include "v_sincos_common.h"

/* sincos not available for all scalar libm implementations.  */
#if defined(_MSC_VER) || !defined(__GLIBC__)
static void
sincos (double x, double *out_sin, double *out_cos)
{
  *out_sin = sin (x);
  *out_cos = cos (x);
}
#endif

static void VPCS_ATTR NOINLINE
special_case (float64x2_t x, uint64x2_t special, double *out_sin,
	      double *out_cos)
{
  if (special[0])
    sincos (x[0], out_sin, out_cos);
  if (special[1])
    sincos (x[1], out_sin + 1, out_cos + 1);
}

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate polynomials.
   Largest observed error is for sin, 3.22 ULP:
   v_sincos_sin (0x1.d70eef40f39b1p+12) got -0x1.ffe9537d5dbb7p-3
				       want -0x1.ffe9537d5dbb4p-3.  */
VPCS_ATTR void
_ZGVnN2vl8l8_sincos (float64x2_t x, double *out_sin, double *out_cos)
{
  const struct v_sincos_data *d = ptr_barrier (&v_sincos_data);
  uint64x2_t special = check_ge_rangeval (x, d);

  float64x2x2_t sc = v_sincos_inline (x, d);

  vst1q_f64 (out_sin, sc.val[0]);
  vst1q_f64 (out_cos, sc.val[1]);

  if (unlikely (v_any_u64 (special)))
    special_case (x, special, out_sin, out_cos);
}

TEST_DISABLE_FENV (_ZGVnN2v_sincos_cos)
TEST_DISABLE_FENV (_ZGVnN2v_sincos_sin)
TEST_ULP (_ZGVnN2v_sincos_sin, 2.73)
TEST_ULP (_ZGVnN2v_sincos_cos, 2.73)
#define V_SINCOS_INTERVAL(lo, hi, n)                                          \
  TEST_INTERVAL (_ZGVnN2v_sincos_sin, lo, hi, n)                              \
  TEST_INTERVAL (_ZGVnN2v_sincos_cos, lo, hi, n)
V_SINCOS_INTERVAL (0, 0x1p-31, 50000)
V_SINCOS_INTERVAL (0x1p-31, 0x1p23, 500000)
V_SINCOS_INTERVAL (0x1p23, inf, 10000)
