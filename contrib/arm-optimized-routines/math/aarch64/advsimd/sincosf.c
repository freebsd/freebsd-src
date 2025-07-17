/*
 * Single-precision vector sincos function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* Define _GNU_SOURCE in order to include sincosf declaration. If building
   pre-GLIBC 2.1, or on a non-GNU conforming system, this routine will need to
   be linked against the scalar sincosf from math/.  */
#define _GNU_SOURCE
#include <math.h>

#include "v_sincosf_common.h"
#include "v_math.h"
#include "test_defs.h"

/* sincos not available for all scalar libm implementations.  */
#if defined(_MSC_VER) || !defined(__GLIBC__)
static void
sincosf (float x, float *out_sin, float *out_cos)
{
  *out_sin = sinf (x);
  *out_cos = cosf (x);
}
#endif

static void VPCS_ATTR NOINLINE
special_case (float32x4_t x, uint32x4_t special, float *out_sin,
	      float *out_cos)
{
  for (int i = 0; i < 4; i++)
    if (special[i])
      sincosf (x[i], out_sin + i, out_cos + i);
}

/* Single-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate low-order
   polynomials.
   Worst-case error for sin is 1.67 ULP:
   v_sincosf_sin(0x1.c704c4p+19) got 0x1.fff698p-5 want 0x1.fff69cp-5
   Worst-case error for cos is 1.81 ULP:
   v_sincosf_cos(0x1.e506fp+19) got -0x1.ffec6ep-6 want -0x1.ffec72p-6.  */
VPCS_ATTR void
_ZGVnN4vl4l4_sincosf (float32x4_t x, float *out_sin, float *out_cos)
{
  const struct v_sincosf_data *d = ptr_barrier (&v_sincosf_data);
  uint32x4_t special = check_ge_rangeval (x, d);

  float32x4x2_t sc = v_sincosf_inline (x, d);

  vst1q_f32 (out_sin, sc.val[0]);
  vst1q_f32 (out_cos, sc.val[1]);

  if (unlikely (v_any_u32 (special)))
    special_case (x, special, out_sin, out_cos);
}

TEST_DISABLE_FENV (_ZGVnN4v_sincosf_sin)
TEST_DISABLE_FENV (_ZGVnN4v_sincosf_cos)
TEST_ULP (_ZGVnN4v_sincosf_sin, 1.17)
TEST_ULP (_ZGVnN4v_sincosf_cos, 1.31)
#define V_SINCOSF_INTERVAL(lo, hi, n)                                         \
  TEST_INTERVAL (_ZGVnN4v_sincosf_sin, lo, hi, n)                             \
  TEST_INTERVAL (_ZGVnN4v_sincosf_cos, lo, hi, n)
V_SINCOSF_INTERVAL (0, 0x1p-31, 50000)
V_SINCOSF_INTERVAL (0x1p-31, 0x1p20, 500000)
V_SINCOSF_INTERVAL (0x1p20, inf, 10000)
