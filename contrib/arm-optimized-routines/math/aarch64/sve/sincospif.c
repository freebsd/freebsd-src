/*
 * Single-precision SVE sincospi(x, *y, *z) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_defs.h"
#include "mathlib.h"
#include "sv_sincospif_common.h"

/* Single-precision vector function allowing calculation of both sinpi and
   cospi in one function call, using shared argument reduction and polynomials.
   Worst-case error for sin is 3.04 ULP:
   _ZGVsMxvl4l4_sincospif_sin(0x1.b51b8p-2) got 0x1.f28b5ep-1 want
   0x1.f28b58p-1.
   Worst-case error for cos is 3.18 ULP:
   _ZGVsMxvl4l4_sincospif_cos(0x1.d341a8p-5) got 0x1.f7cd56p-1 want
   0x1.f7cd5p-1.  */
void
_ZGVsMxvl4l4_sincospif (svfloat32_t x, float *out_sin, float *out_cos,
			svbool_t pg)
{
  const struct sv_sincospif_data *d = ptr_barrier (&sv_sincospif_data);

  svfloat32x2_t sc = sv_sincospif_inline (pg, x, d);

  svst1 (pg, out_sin, svget2 (sc, 0));
  svst1 (pg, out_cos, svget2 (sc, 1));
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (_ZGVsMxvl4l4_sincospif_sin)
TEST_DISABLE_FENV (_ZGVsMxvl4l4_sincospif_cos)
TEST_ULP (_ZGVsMxvl4l4_sincospif_sin, 2.54)
TEST_ULP (_ZGVsMxvl4l4_sincospif_cos, 2.68)
#  define SV_SINCOSPIF_INTERVAL(lo, hi, n)                                    \
    TEST_SYM_INTERVAL (_ZGVsMxvl4l4_sincospif_sin, lo, hi, n)                 \
    TEST_SYM_INTERVAL (_ZGVsMxvl4l4_sincospif_cos, lo, hi, n)
SV_SINCOSPIF_INTERVAL (0, 0x1p-31, 10000)
SV_SINCOSPIF_INTERVAL (0x1p-31, 0.5, 50000)
SV_SINCOSPIF_INTERVAL (0.5, 0x1p31, 50000)
SV_SINCOSPIF_INTERVAL (0x1p31, inf, 10000)
#endif
CLOSE_SVE_ATTR
