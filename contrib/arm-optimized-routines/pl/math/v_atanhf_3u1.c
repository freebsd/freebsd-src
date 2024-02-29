/*
 * Single-precision vector atanh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
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
special_case (float32x4_t x, float32x4_t y, uint32x4_t special)
{
  return v_call_f32 (atanhf, x, y, special);
}

/* Approximation for vector single-precision atanh(x) using modified log1p.
   The maximum error is 3.08 ULP:
   __v_atanhf(0x1.ff215p-5) got 0x1.ffcb7cp-5
			   want 0x1.ffcb82p-5.  */
VPCS_ATTR float32x4_t V_NAME_F1 (atanh) (float32x4_t x)
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

  float32x4_t y = vdivq_f32 (vaddq_f32 (ax, ax), vsubq_f32 (v_f32 (1), ax));
  y = log1pf_inline (y, d->log1pf_consts);

  if (unlikely (v_any_u32 (special)))
    return special_case (x, vmulq_f32 (halfsign, y), special);
  return vmulq_f32 (halfsign, y);
}

PL_SIG (V, F, 1, atanh, -1.0, 1.0)
PL_TEST_ULP (V_NAME_F1 (atanh), 2.59)
PL_TEST_EXPECT_FENV (V_NAME_F1 (atanh), WANT_SIMD_EXCEPT)
/* atanh is asymptotic at 1, which is the default control value - have to set
 -c 0 specially to ensure fp exceptions are triggered correctly (choice of
 control lane is irrelevant if fp exceptions are disabled).  */
PL_TEST_SYM_INTERVAL_C (V_NAME_F1 (atanh), 0, 0x1p-12, 500, 0)
PL_TEST_SYM_INTERVAL_C (V_NAME_F1 (atanh), 0x1p-12, 1, 200000, 0)
PL_TEST_SYM_INTERVAL_C (V_NAME_F1 (atanh), 1, inf, 1000, 0)
