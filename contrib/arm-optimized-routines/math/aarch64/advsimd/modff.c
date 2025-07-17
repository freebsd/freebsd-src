/*
 * Single-precision vector modf(x, *y) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

/* Modff algorithm. Produces exact values in all rounding modes.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1_L1 (modf) (float32x4_t x,
						    float *out_int)
{
  /* Get integer component of x.  */
  float32x4_t rounded = vrndq_f32 (x);
  vst1q_f32 (out_int, rounded);

  /* Subtract integer component from input.  */
  uint32x4_t remaining = vreinterpretq_u32_f32 (vsubq_f32 (x, rounded));

  /* Return +0 for integer x.  */
  uint32x4_t is_integer = vceqq_f32 (x, rounded);
  return vreinterpretq_f32_u32 (vbicq_u32 (remaining, is_integer));
}

TEST_ULP (_ZGVnN4vl4_modff_frac, 0.0)
TEST_SYM_INTERVAL (_ZGVnN4vl4_modff_frac, 0, 1, 20000)
TEST_SYM_INTERVAL (_ZGVnN4vl4_modff_frac, 1, inf, 20000)

TEST_ULP (_ZGVnN4vl4_modff_int, 0.0)
TEST_SYM_INTERVAL (_ZGVnN4vl4_modff_int, 0, 1, 20000)
TEST_SYM_INTERVAL (_ZGVnN4vl4_modff_int, 1, inf, 20000)
