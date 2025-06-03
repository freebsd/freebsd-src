/*
 * Single-precision SVE acosh(x) function.
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

#define One 0x3f800000
#define Thres 0x20000000 /* asuint(0x1p64) - One.  */

#include "sv_log1pf_inline.h"

static svfloat32_t NOINLINE
special_case (svfloat32_t xm1, svfloat32_t tmp, svbool_t special)
{
  svfloat32_t x = svadd_x (svptrue_b32 (), xm1, 1.0f);
  svfloat32_t y = sv_log1pf_inline (tmp, svptrue_b32 ());
  return sv_call_f32 (acoshf, x, y, special);
}

/* Single-precision SVE acosh(x) routine. Implements the same algorithm as
   vector acoshf and log1p.

   Maximum error is 2.47 ULPs:
   SV_NAME_F1 (acosh) (0x1.01ca76p+0) got 0x1.e435a6p-4
				     want 0x1.e435a2p-4.  */
svfloat32_t SV_NAME_F1 (acosh) (svfloat32_t x, const svbool_t pg)
{
  svuint32_t ix = svreinterpret_u32 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, ix, One), Thres);

  svfloat32_t xm1 = svsub_x (pg, x, 1.0f);
  svfloat32_t u = svmul_x (pg, xm1, svadd_x (pg, x, 1.0f));
  svfloat32_t tmp = svadd_x (pg, xm1, svsqrt_x (pg, u));

  if (unlikely (svptest_any (pg, special)))
    return special_case (xm1, tmp, special);
  return sv_log1pf_inline (tmp, pg);
}

TEST_SIG (SV, F, 1, acosh, 1.0, 10.0)
TEST_ULP (SV_NAME_F1 (acosh), 1.97)
TEST_DISABLE_FENV (SV_NAME_F1 (acosh))
TEST_INTERVAL (SV_NAME_F1 (acosh), 0, 1, 500)
TEST_INTERVAL (SV_NAME_F1 (acosh), 1, 0x1p64, 100000)
TEST_INTERVAL (SV_NAME_F1 (acosh), 0x1p64, inf, 1000)
TEST_INTERVAL (SV_NAME_F1 (acosh), -0, -inf, 1000)
CLOSE_SVE_ATTR
