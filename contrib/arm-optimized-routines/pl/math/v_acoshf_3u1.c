/*
 * Single-precision vector acosh(x) function.
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "v_log1pf_inline.h"

const static struct data
{
  struct v_log1pf_data log1pf_consts;
  uint32x4_t one;
  uint16x4_t thresh;
} data = {
  .log1pf_consts = V_LOG1PF_CONSTANTS_TABLE,
  .one = V4 (0x3f800000),
  .thresh = V4 (0x2000) /* asuint(0x1p64) - asuint(1).  */
};

#define SignMask 0x80000000

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t y, uint16x4_t special,
	      const struct v_log1pf_data d)
{
  return v_call_f32 (acoshf, x, log1pf_inline (y, d), vmovl_u16 (special));
}

/* Vector approximation for single-precision acosh, based on log1p. Maximum
   error depends on WANT_SIMD_EXCEPT. With SIMD fp exceptions enabled, it
   is 2.78 ULP:
   __v_acoshf(0x1.07887p+0) got 0x1.ef9e9cp-3
			   want 0x1.ef9ea2p-3.
   With exceptions disabled, we can compute u with a shorter dependency chain,
   which gives maximum error of 3.07 ULP:
  __v_acoshf(0x1.01f83ep+0) got 0x1.fbc7fap-4
			   want 0x1.fbc7f4p-4.  */

VPCS_ATTR float32x4_t V_NAME_F1 (acosh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint16x4_t special = vcge_u16 (vsubhn_u32 (ix, d->one), d->thresh);

#if WANT_SIMD_EXCEPT
  /* Mask special lanes with 1 to side-step spurious invalid or overflow. Use
     only xm1 to calculate u, as operating on x will trigger invalid for NaN.
     Widening sign-extend special predicate in order to mask with it.  */
  uint32x4_t p
      = vreinterpretq_u32_s32 (vmovl_s16 (vreinterpret_s16_u16 (special)));
  float32x4_t xm1 = v_zerofy_f32 (vsubq_f32 (x, v_f32 (1)), p);
  float32x4_t u = vfmaq_f32 (vaddq_f32 (xm1, xm1), xm1, xm1);
#else
  float32x4_t xm1 = vsubq_f32 (x, v_f32 (1));
  float32x4_t u = vmulq_f32 (xm1, vaddq_f32 (x, v_f32 (1.0f)));
#endif

  float32x4_t y = vaddq_f32 (xm1, vsqrtq_f32 (u));

  if (unlikely (v_any_u16h (special)))
    return special_case (x, y, special, d->log1pf_consts);
  return log1pf_inline (y, d->log1pf_consts);
}

PL_SIG (V, F, 1, acosh, 1.0, 10.0)
#if WANT_SIMD_EXCEPT
PL_TEST_ULP (V_NAME_F1 (acosh), 2.29)
#else
PL_TEST_ULP (V_NAME_F1 (acosh), 2.58)
#endif
PL_TEST_EXPECT_FENV (V_NAME_F1 (acosh), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_F1 (acosh), 0, 1, 500)
PL_TEST_INTERVAL (V_NAME_F1 (acosh), 1, SquareLim, 100000)
PL_TEST_INTERVAL (V_NAME_F1 (acosh), SquareLim, inf, 1000)
PL_TEST_INTERVAL (V_NAME_F1 (acosh), -0, -inf, 1000)
