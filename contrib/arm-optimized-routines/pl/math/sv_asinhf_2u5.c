/*
 * Single-precision SVE asinh(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "include/mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#include "sv_log1pf_inline.h"

#define BigBound (0x5f800000)  /* asuint(0x1p64).  */

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (asinhf, x, y, special);
}

/* Single-precision SVE asinh(x) routine. Implements the same algorithm as
   vector asinhf and log1p.

   Maximum error is 2.48 ULPs:
   SV_NAME_F1 (asinh) (0x1.008864p-3) got 0x1.ffbbbcp-4
				     want 0x1.ffbbb8p-4.  */
svfloat32_t SV_NAME_F1 (asinh) (svfloat32_t x, const svbool_t pg)
{
  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);
  svbool_t special = svcmpge (pg, iax, BigBound);

  /* asinh(x) = log(x + sqrt(x * x + 1)).
     For positive x, asinh(x) = log1p(x + x * x / (1 + sqrt(x * x + 1))).  */
  svfloat32_t ax2 = svmul_x (pg, ax, ax);
  svfloat32_t d = svadd_x (pg, svsqrt_x (pg, svadd_x (pg, ax2, 1.0f)), 1.0f);
  svfloat32_t y
      = sv_log1pf_inline (svadd_x (pg, ax, svdiv_x (pg, ax2, d)), pg);

  if (unlikely (svptest_any (pg, special)))
    return special_case (
	x, svreinterpret_f32 (svorr_x (pg, sign, svreinterpret_u32 (y))),
	special);
  return svreinterpret_f32 (svorr_x (pg, sign, svreinterpret_u32 (y)));
}

PL_SIG (SV, F, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_F1 (asinh), 1.98)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (asinh), 0, 0x1p-12, 4000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (asinh), 0x1p-12, 1.0, 20000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (asinh), 1.0, 0x1p64, 20000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (asinh), 0x1p64, inf, 4000)
