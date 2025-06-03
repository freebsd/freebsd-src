/*
 * Single-precision vector atanh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_log1pf_inline.h"

const static struct data
{
  struct v_log1pf_data log1pf_consts;
  uint32x4_t one;
#if WANT_SIMD_EXCEPT
  uint32x4_t tiny_bound;
#endif
} data = {
  .log1pf_consts = V_LOG1PF_CONSTANTS_TABLE,
  .one = V4 (0x3f800000),
#if WANT_SIMD_EXCEPT
  /* 0x1p-12, below which atanhf(x) rounds to x.  */
  .tiny_bound = V4 (0x39800000),
#endif
};

#define AbsMask v_u32 (0x7fffffff)
#define Half v_u32 (0x3f000000)

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t halfsign, float32x4_t y,
	      uint32x4_t special)
{
  return v_call_f32 (atanhf, vbslq_f32 (AbsMask, x, halfsign),
		     vmulq_f32 (halfsign, y), special);
}

/* Approximation for vector single-precision atanh(x) using modified log1p.
   The maximum error is 2.93 ULP:
   _ZGVnN4v_atanhf(0x1.f43d7p-5) got 0x1.f4dcfep-5
				want 0x1.f4dcf8p-5.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (atanh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  float32x4_t halfsign = vbslq_f32 (AbsMask, v_f32 (0.5), x);
  float32x4_t ax = vabsq_f32 (x);
  uint32x4_t iax = vreinterpretq_u32_f32 (ax);

#if WANT_SIMD_EXCEPT
  uint32x4_t special
      = vorrq_u32 (vcgeq_u32 (iax, d->one), vcltq_u32 (iax, d->tiny_bound));
  /* Side-step special cases by setting those lanes to 0, which will trigger no
     exceptions. These will be fixed up later.  */
  if (unlikely (v_any_u32 (special)))
    ax = v_zerofy_f32 (ax, special);
#else
  uint32x4_t special = vcgeq_u32 (iax, d->one);
#endif

  float32x4_t y = vdivq_f32 (vaddq_f32 (ax, ax),
			     vsubq_f32 (vreinterpretq_f32_u32 (d->one), ax));
  y = log1pf_inline (y, &d->log1pf_consts);

  /* If exceptions not required, pass ax to special-case for shorter dependency
     chain. If exceptions are required ax will have been zerofied, so have to
     pass x.  */
  if (unlikely (v_any_u32 (special)))
#if WANT_SIMD_EXCEPT
    return special_case (x, halfsign, y, special);
#else
    return special_case (ax, halfsign, y, special);
#endif
  return vmulq_f32 (halfsign, y);
}

HALF_WIDTH_ALIAS_F1 (atanh)

TEST_SIG (V, F, 1, atanh, -1.0, 1.0)
TEST_ULP (V_NAME_F1 (atanh), 2.44)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (atanh), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (atanh), 0, 0x1p-12, 500)
TEST_SYM_INTERVAL (V_NAME_F1 (atanh), 0x1p-12, 1, 200000)
TEST_SYM_INTERVAL (V_NAME_F1 (atanh), 1, inf, 1000)
/* atanh is asymptotic at 1, which is the default control value - have to set
 -c 0 specially to ensure fp exceptions are triggered correctly (choice of
 control lane is irrelevant if fp exceptions are disabled).  */
TEST_CONTROL_VALUE (V_NAME_F1 (atanh), 0)
