/*
 * Single-precision vector sincospi function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_sincospif_common.h"
#include "v_math.h"
#include "test_defs.h"
#include "mathlib.h"

/* Single-precision vector function allowing calculation of both sinpi and
   cospi in one function call, using shared argument reduction and polynomials.
   Worst-case error for sin is 3.04 ULP:
   _ZGVnN4v_sincospif_sin(0x1.1d341ap-1) got 0x1.f7cd56p-1 want 0x1.f7cd5p-1.
   Worst-case error for cos is 3.18 ULP:
   _ZGVnN4v_sincospif_cos(0x1.d341a8p-5) got 0x1.f7cd56p-1 want 0x1.f7cd5p-1.
 */
VPCS_ATTR void
_ZGVnN4vl4l4_sincospif (float32x4_t x, float *out_sin, float *out_cos)
{
  const struct v_sincospif_data *d = ptr_barrier (&v_sincospif_data);

  float32x4x2_t sc = v_sincospif_inline (x, d);

  vst1q_f32 (out_sin, sc.val[0]);
  vst1q_f32 (out_cos, sc.val[1]);
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (_ZGVnN4v_sincospif_sin)
TEST_DISABLE_FENV (_ZGVnN4v_sincospif_cos)
TEST_ULP (_ZGVnN4v_sincospif_sin, 2.54)
TEST_ULP (_ZGVnN4v_sincospif_cos, 2.68)
#  define V_SINCOSPIF_INTERVAL(lo, hi, n)                                     \
    TEST_SYM_INTERVAL (_ZGVnN4v_sincospif_sin, lo, hi, n)                     \
    TEST_SYM_INTERVAL (_ZGVnN4v_sincospif_cos, lo, hi, n)
V_SINCOSPIF_INTERVAL (0, 0x1p-63, 10000)
V_SINCOSPIF_INTERVAL (0x1p-63, 0.5, 50000)
V_SINCOSPIF_INTERVAL (0.5, 0x1p31, 50000)
V_SINCOSPIF_INTERVAL (0x1p31, inf, 10000)
#endif
