/*
 * Double-precision vector atanh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#define WANT_V_LOG1P_K0_SHORTCUT 0
#include "v_log1p_inline.h"

const static struct data
{
  struct v_log1p_data log1p_consts;
  uint64x2_t one, half;
} data = { .log1p_consts = V_LOG1P_CONSTANTS_TABLE,
	   .one = V2 (0x3ff0000000000000),
	   .half = V2 (0x3fe0000000000000) };

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (atanh, x, y, special);
}

/* Approximation for vector double-precision atanh(x) using modified log1p.
   The greatest observed error is 3.31 ULP:
   _ZGVnN2v_atanh(0x1.ffae6288b601p-6) got 0x1.ffd8ff31b5019p-6
				      want 0x1.ffd8ff31b501cp-6.  */
VPCS_ATTR
float64x2_t V_NAME_D1 (atanh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t ia = vreinterpretq_u64_f64 (ax);
  uint64x2_t sign = veorq_u64 (vreinterpretq_u64_f64 (x), ia);
  uint64x2_t special = vcgeq_u64 (ia, d->one);
  float64x2_t halfsign = vreinterpretq_f64_u64 (vorrq_u64 (sign, d->half));

#if WANT_SIMD_EXCEPT
  ax = v_zerofy_f64 (ax, special);
#endif

  float64x2_t y;
  y = vaddq_f64 (ax, ax);
  y = vdivq_f64 (y, vsubq_f64 (v_f64 (1), ax));
  y = log1p_inline (y, &d->log1p_consts);

  if (unlikely (v_any_u64 (special)))
    return special_case (x, vmulq_f64 (y, halfsign), special);
  return vmulq_f64 (y, halfsign);
}

PL_SIG (V, D, 1, atanh, -1.0, 1.0)
PL_TEST_EXPECT_FENV (V_NAME_D1 (atanh), WANT_SIMD_EXCEPT)
PL_TEST_ULP (V_NAME_D1 (atanh), 3.32)
/* atanh is asymptotic at 1, which is the default control value - have to set
   -c 0 specially to ensure fp exceptions are triggered correctly (choice of
   control lane is irrelevant if fp exceptions are disabled).  */
PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (atanh), 0, 0x1p-23, 10000, 0)
PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (atanh), 0x1p-23, 1, 90000, 0)
PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (atanh), 1, inf, 100, 0)
