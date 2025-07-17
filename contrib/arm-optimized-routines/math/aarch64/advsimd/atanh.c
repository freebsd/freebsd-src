/*
 * Double-precision vector atanh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define WANT_V_LOG1P_K0_SHORTCUT 0
#include "v_log1p_inline.h"

const static struct data
{
  struct v_log1p_data log1p_consts;
  uint64x2_t one;
  uint64x2_t sign_mask;
} data = { .log1p_consts = V_LOG1P_CONSTANTS_TABLE,
	   .one = V2 (0x3ff0000000000000),
	   .sign_mask = V2 (0x8000000000000000) };

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t halfsign, float64x2_t y,
	      uint64x2_t special, const struct data *d)
{
  y = log1p_inline (y, &d->log1p_consts);
  return v_call_f64 (atanh, vbslq_f64 (d->sign_mask, halfsign, x),
		     vmulq_f64 (halfsign, y), special);
}

/* Approximation for vector double-precision atanh(x) using modified log1p.
   The greatest observed error is 3.31 ULP:
   _ZGVnN2v_atanh(0x1.ffae6288b601p-6) got 0x1.ffd8ff31b5019p-6
				      want 0x1.ffd8ff31b501cp-6.  */
VPCS_ATTR
float64x2_t V_NAME_D1 (atanh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t halfsign = vbslq_f64 (d->sign_mask, x, v_f64 (0.5));
  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t ia = vreinterpretq_u64_f64 (ax);
  uint64x2_t special = vcgeq_u64 (ia, d->one);

#if WANT_SIMD_EXCEPT
  ax = v_zerofy_f64 (ax, special);
#endif

  float64x2_t y;
  y = vaddq_f64 (ax, ax);
  y = vdivq_f64 (y, vsubq_f64 (vreinterpretq_f64_u64 (d->one), ax));

  if (unlikely (v_any_u64 (special)))
#if WANT_SIMD_EXCEPT
    return special_case (x, halfsign, y, special, d);
#else
    return special_case (ax, halfsign, y, special, d);
#endif

  y = log1p_inline (y, &d->log1p_consts);
  return vmulq_f64 (y, halfsign);
}

TEST_SIG (V, D, 1, atanh, -1.0, 1.0)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (atanh), WANT_SIMD_EXCEPT)
TEST_ULP (V_NAME_D1 (atanh), 3.32)
TEST_SYM_INTERVAL (V_NAME_D1 (atanh), 0, 0x1p-23, 10000)
TEST_SYM_INTERVAL (V_NAME_D1 (atanh), 0x1p-23, 1, 90000)
TEST_SYM_INTERVAL (V_NAME_D1 (atanh), 1, inf, 100)
/* atanh is asymptotic at 1, which is the default control value - have to set
   -c 0 specially to ensure fp exceptions are triggered correctly (choice of
   control lane is irrelevant if fp exceptions are disabled).  */
TEST_CONTROL_VALUE (V_NAME_D1 (atanh), 0)
