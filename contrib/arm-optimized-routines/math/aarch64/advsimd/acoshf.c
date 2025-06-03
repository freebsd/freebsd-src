/*
 * Single-precision vector acosh(x) function.
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_log1pf_inline.h"

#define SquareLim 0x1p64

const static struct data
{
  struct v_log1pf_data log1pf_consts;
  uint32x4_t one;
} data = { .log1pf_consts = V_LOG1PF_CONSTANTS_TABLE, .one = V4 (0x3f800000) };

#define Thresh vdup_n_u16 (0x2000) /* top(asuint(SquareLim) - asuint(1)).  */

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t y, uint16x4_t special,
	      const struct v_log1pf_data *d)
{
  return v_call_f32 (acoshf, x, log1pf_inline (y, d), vmovl_u16 (special));
}

/* Vector approximation for single-precision acosh, based on log1p. Maximum
   error depends on WANT_SIMD_EXCEPT. With SIMD fp exceptions enabled, it
   is 3.00 ULP:
   _ZGVnN4v_acoshf(0x1.01df3ap+0) got 0x1.ef0a82p-4
				 want 0x1.ef0a7cp-4.
   With exceptions disabled, we can compute u with a shorter dependency chain,
   which gives maximum error of 3.22 ULP:
   _ZGVnN4v_acoshf(0x1.007ef2p+0) got 0x1.fdcdccp-5
				 want 0x1.fdcdd2p-5.  */

float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (acosh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint16x4_t special = vcge_u16 (vsubhn_u32 (ix, d->one), Thresh);

#if WANT_SIMD_EXCEPT
  /* Mask special lanes with 1 to side-step spurious invalid or overflow. Use
     only xm1 to calculate u, as operating on x will trigger invalid for NaN.
     Widening sign-extend special predicate in order to mask with it.  */
  uint32x4_t p
      = vreinterpretq_u32_s32 (vmovl_s16 (vreinterpret_s16_u16 (special)));
  float32x4_t xm1 = v_zerofy_f32 (vsubq_f32 (x, v_f32 (1)), p);
  float32x4_t u = vfmaq_f32 (vaddq_f32 (xm1, xm1), xm1, xm1);
#else
  float32x4_t xm1 = vsubq_f32 (x, vreinterpretq_f32_u32 (d->one));
  float32x4_t u
      = vmulq_f32 (xm1, vaddq_f32 (x, vreinterpretq_f32_u32 (d->one)));
#endif

  float32x4_t y = vaddq_f32 (xm1, vsqrtq_f32 (u));

  if (unlikely (v_any_u16h (special)))
    return special_case (x, y, special, &d->log1pf_consts);
  return log1pf_inline (y, &d->log1pf_consts);
}

HALF_WIDTH_ALIAS_F1 (acosh)

TEST_SIG (V, F, 1, acosh, 1.0, 10.0)
#if WANT_SIMD_EXCEPT
TEST_ULP (V_NAME_F1 (acosh), 2.50)
#else
TEST_ULP (V_NAME_F1 (acosh), 2.78)
#endif
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (acosh), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_F1 (acosh), 0, 1, 500)
TEST_INTERVAL (V_NAME_F1 (acosh), 1, SquareLim, 100000)
TEST_INTERVAL (V_NAME_F1 (acosh), SquareLim, inf, 1000)
TEST_INTERVAL (V_NAME_F1 (acosh), -0, -inf, 1000)
