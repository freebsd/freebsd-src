/*
 * Single-precision vector cexpi function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_sincosf_common.h"
#include "v_math.h"
#include "test_defs.h"

static float32x4x2_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, uint32x4_t special, float32x4x2_t y)
{
  return (float32x4x2_t){ v_call_f32 (sinf, x, y.val[0], special),
			  v_call_f32 (cosf, x, y.val[1], special) };
}

/* Single-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate low-order
   polynomials.
   Worst-case error for sin is 1.67 ULP:
   v_cexpif_sin(0x1.c704c4p+19) got 0x1.fff698p-5 want 0x1.fff69cp-5
   Worst-case error for cos is 1.81 ULP:
   v_cexpif_cos(0x1.e506fp+19) got -0x1.ffec6ep-6 want -0x1.ffec72p-6.  */
VPCS_ATTR float32x4x2_t
_ZGVnN4v_cexpif (float32x4_t x)
{
  const struct v_sincosf_data *d = ptr_barrier (&v_sincosf_data);
  uint32x4_t special = check_ge_rangeval (x, d);

  float32x4x2_t sc = v_sincosf_inline (x, d);

  if (unlikely (v_any_u32 (special)))
    return special_case (x, special, sc);
  return sc;
}

TEST_DISABLE_FENV (_ZGVnN4v_cexpif_sin)
TEST_DISABLE_FENV (_ZGVnN4v_cexpif_cos)
TEST_ULP (_ZGVnN4v_cexpif_sin, 1.17)
TEST_ULP (_ZGVnN4v_cexpif_cos, 1.31)
#define V_CEXPIF_INTERVAL(lo, hi, n)                                          \
  TEST_INTERVAL (_ZGVnN4v_cexpif_sin, lo, hi, n)                              \
  TEST_INTERVAL (_ZGVnN4v_cexpif_cos, lo, hi, n)
V_CEXPIF_INTERVAL (0, 0x1p20, 500000)
V_CEXPIF_INTERVAL (-0, -0x1p20, 500000)
V_CEXPIF_INTERVAL (0x1p20, inf, 10000)
V_CEXPIF_INTERVAL (-0x1p20, -inf, 10000)
