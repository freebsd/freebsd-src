/*
 * Single-precision vector atanh(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#include "sv_log1pf_inline.h"

#define One (0x3f800000)
#define Half (0x3f000000)

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (atanhf, x, y, special);
}

/* Approximation for vector single-precision atanh(x) using modified log1p.
   The maximum error is 2.28 ULP:
   _ZGVsMxv_atanhf(0x1.ff1194p-5) got 0x1.ffbbbcp-5
				 want 0x1.ffbbb6p-5.  */
svfloat32_t SV_NAME_F1 (atanh) (svfloat32_t x, const svbool_t pg)
{
  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);
  svfloat32_t halfsign = svreinterpret_f32 (svorr_x (pg, sign, Half));
  svbool_t special = svcmpge (pg, iax, One);

  /* Computation is performed based on the following sequence of equality:
   * (1+x)/(1-x) = 1 + 2x/(1-x).  */
  svfloat32_t y = svadd_x (pg, ax, ax);
  y = svdiv_x (pg, y, svsub_x (pg, sv_f32 (1), ax));
  /* ln((1+x)/(1-x)) = ln(1+2x/(1-x)) = ln(1 + y).  */
  y = sv_log1pf_inline (y, pg);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svmul_x (pg, halfsign, y), special);

  return svmul_x (pg, halfsign, y);
}

PL_SIG (SV, F, 1, atanh, -1.0, 1.0)
PL_TEST_ULP (SV_NAME_F1 (atanh), 2.59)
/* atanh is asymptotic at 1, which is the default control value - have to set
 -c 0 specially to ensure fp exceptions are triggered correctly (choice of
 control lane is irrelevant if fp exceptions are disabled).  */
PL_TEST_SYM_INTERVAL_C (SV_NAME_F1 (atanh), 0, 0x1p-12, 1000, 0)
PL_TEST_SYM_INTERVAL_C (SV_NAME_F1 (atanh), 0x1p-12, 1, 20000, 0)
PL_TEST_SYM_INTERVAL_C (SV_NAME_F1 (atanh), 1, inf, 1000, 0)
