/*
 * Double-precision vector acosh(x) function.
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define WANT_V_LOG1P_K0_SHORTCUT 1
#include "v_log1p_inline.h"

const static struct data
{
  struct v_log1p_data log1p_consts;
  uint64x2_t one, thresh;
} data = {
  .log1p_consts = V_LOG1P_CONSTANTS_TABLE,
  .one = V2 (0x3ff0000000000000),
  .thresh = V2 (0x1ff0000000000000) /* asuint64(0x1p511) - asuint64(1).  */
};

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t special,
	      const struct v_log1p_data *d)
{
  return v_call_f64 (acosh, x, log1p_inline (y, d), special);
}

/* Vector approximation for double-precision acosh, based on log1p.
   The largest observed error is 3.02 ULP in the region where the
   argument to log1p falls in the k=0 interval, i.e. x close to 1:
   _ZGVnN2v_acosh(0x1.00798aaf80739p+0) got 0x1.f2d6d823bc9dfp-5
				       want 0x1.f2d6d823bc9e2p-5.  */
VPCS_ATTR float64x2_t V_NAME_D1 (acosh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t special
      = vcgeq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (x), d->one), d->thresh);
  float64x2_t special_arg = x;

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (special)))
    x = vbslq_f64 (special, vreinterpretq_f64_u64 (d->one), x);
#endif

  float64x2_t xm1 = vsubq_f64 (x, v_f64 (1.0));
  float64x2_t y = vaddq_f64 (x, v_f64 (1.0));
  y = vmulq_f64 (y, xm1);
  y = vsqrtq_f64 (y);
  y = vaddq_f64 (xm1, y);

  if (unlikely (v_any_u64 (special)))
    return special_case (special_arg, y, special, &d->log1p_consts);
  return log1p_inline (y, &d->log1p_consts);
}

TEST_SIG (V, D, 1, acosh, 1.0, 10.0)
TEST_ULP (V_NAME_D1 (acosh), 2.53)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (acosh), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_D1 (acosh), 1, 0x1p511, 90000)
TEST_INTERVAL (V_NAME_D1 (acosh), 0x1p511, inf, 10000)
TEST_INTERVAL (V_NAME_D1 (acosh), 0, 1, 1000)
TEST_INTERVAL (V_NAME_D1 (acosh), -0, -inf, 10000)
