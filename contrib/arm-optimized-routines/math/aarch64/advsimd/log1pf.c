/*
 * Single-precision vector log(1+x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_log1pf_inline.h"

#if WANT_SIMD_EXCEPT

const static struct data
{
  uint32x4_t minus_one, thresh;
  struct v_log1pf_data d;
} data = {
  .d = V_LOG1PF_CONSTANTS_TABLE,
  .thresh = V4 (0x4b800000), /* asuint32(INFINITY) - TinyBound.  */
  .minus_one = V4 (0xbf800000),
};

/* asuint32(0x1p-23). ulp=0.5 at 0x1p-23.  */
#  define TinyBound v_u32 (0x34000000)

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, uint32x4_t cmp, const struct data *d)
{
  /* Side-step special lanes so fenv exceptions are not triggered
     inadvertently.  */
  float32x4_t x_nospecial = v_zerofy_f32 (x, cmp);
  return v_call_f32 (log1pf, x, log1pf_inline (x_nospecial, &d->d), cmp);
}

/* Vector log1pf approximation using polynomial on reduced interval. Worst-case
   error is 1.69 ULP:
   _ZGVnN4v_log1pf(0x1.04418ap-2) got 0x1.cfcbd8p-3
				 want 0x1.cfcbdcp-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log1p) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t ia = vreinterpretq_u32_f32 (vabsq_f32 (x));

  uint32x4_t special_cases
      = vorrq_u32 (vcgeq_u32 (vsubq_u32 (ia, TinyBound), d->thresh),
		   vcgeq_u32 (ix, d->minus_one));

  if (unlikely (v_any_u32 (special_cases)))
    return special_case (x, special_cases, d);

  return log1pf_inline (x, &d->d);
}

#else

const static struct v_log1pf_data data = V_LOG1PF_CONSTANTS_TABLE;

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, uint32x4_t cmp)
{
  return v_call_f32 (log1pf, x, log1pf_inline (x, ptr_barrier (&data)), cmp);
}

/* Vector log1pf approximation using polynomial on reduced interval. Worst-case
   error is 1.63 ULP:
   _ZGVnN4v_log1pf(0x1.216d12p-2) got 0x1.fdcb12p-3
				 want 0x1.fdcb16p-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log1p) (float32x4_t x)
{
  uint32x4_t special_cases = vornq_u32 (vcleq_f32 (x, v_f32 (-1)),
					vcaleq_f32 (x, v_f32 (0x1p127f)));

  if (unlikely (v_any_u32 (special_cases)))
    return special_case (x, special_cases);

  return log1pf_inline (x, ptr_barrier (&data));
}

#endif

HALF_WIDTH_ALIAS_F1 (log1p)

TEST_SIG (V, F, 1, log1p, -0.9, 10.0)
TEST_ULP (V_NAME_F1 (log1p), 1.20)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (log1p), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (log1p), 0.0, 0x1p-23, 30000)
TEST_SYM_INTERVAL (V_NAME_F1 (log1p), 0x1p-23, 1, 50000)
TEST_INTERVAL (V_NAME_F1 (log1p), 1, inf, 50000)
TEST_INTERVAL (V_NAME_F1 (log1p), -1.0, -inf, 1000)
