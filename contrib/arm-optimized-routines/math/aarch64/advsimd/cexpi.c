/*
 * Double-precision vector sincos function - return-by-value interface.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_sincos_common.h"
#include "v_math.h"
#include "test_defs.h"

static float64x2x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, uint64x2_t special, float64x2x2_t y)
{
  return (float64x2x2_t){ v_call_f64 (sin, x, y.val[0], special),
			  v_call_f64 (cos, x, y.val[1], special) };
}

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate polynomials.
   Largest observed error is for sin, 3.22 ULP:
   v_sincos_sin (0x1.d70eef40f39b1p+12) got -0x1.ffe9537d5dbb7p-3
				       want -0x1.ffe9537d5dbb4p-3.  */
VPCS_ATTR float64x2x2_t
_ZGVnN2v_cexpi (float64x2_t x)
{
  const struct v_sincos_data *d = ptr_barrier (&v_sincos_data);
  uint64x2_t special = check_ge_rangeval (x, d);

  float64x2x2_t sc = v_sincos_inline (x, d);

  if (unlikely (v_any_u64 (special)))
    return special_case (x, special, sc);
  return sc;
}

TEST_DISABLE_FENV (_ZGVnN2v_cexpi_cos)
TEST_DISABLE_FENV (_ZGVnN2v_cexpi_sin)
TEST_ULP (_ZGVnN2v_cexpi_sin, 2.73)
TEST_ULP (_ZGVnN2v_cexpi_cos, 2.73)
#define V_CEXPI_INTERVAL(lo, hi, n)                                           \
  TEST_INTERVAL (_ZGVnN2v_cexpi_sin, lo, hi, n)                               \
  TEST_INTERVAL (_ZGVnN2v_cexpi_cos, lo, hi, n)
V_CEXPI_INTERVAL (0, 0x1p23, 500000)
V_CEXPI_INTERVAL (-0, -0x1p23, 500000)
V_CEXPI_INTERVAL (0x1p23, inf, 10000)
V_CEXPI_INTERVAL (-0x1p23, -inf, 10000)
