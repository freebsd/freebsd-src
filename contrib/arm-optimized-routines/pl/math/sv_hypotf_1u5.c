/*
 * Single-precision SVE hypot(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#define TinyBound 0x0c800000 /* asuint (0x1p-102).  */
#define Thres 0x73000000     /* 0x70000000 - TinyBound.  */

static svfloat32_t NOINLINE
special_case (svfloat32_t sqsum, svfloat32_t x, svfloat32_t y, svbool_t pg,
	      svbool_t special)
{
  return sv_call2_f32 (hypotf, x, y, svsqrt_x (pg, sqsum), special);
}

/* SVE implementation of single-precision hypot.
   Maximum error observed is 1.21 ULP:
   _ZGVsMxvv_hypotf (0x1.6a213cp-19, -0x1.32b982p-26) got 0x1.6a2346p-19
						     want 0x1.6a2344p-19.  */
svfloat32_t SV_NAME_F2 (hypot) (svfloat32_t x, svfloat32_t y,
				const svbool_t pg)
{
  svfloat32_t sqsum = svmla_x (pg, svmul_x (pg, x, x), y, y);

  svbool_t special = svcmpge (
      pg, svsub_x (pg, svreinterpret_u32 (sqsum), TinyBound), Thres);

  if (unlikely (svptest_any (pg, special)))
    return special_case (sqsum, x, y, pg, special);

  return svsqrt_x (pg, sqsum);
}

PL_SIG (SV, F, 2, hypot, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_F2 (hypot), 0.71)
PL_TEST_INTERVAL2 (SV_NAME_F2 (hypot), 0, inf, 0, inf, 10000)
PL_TEST_INTERVAL2 (SV_NAME_F2 (hypot), 0, inf, -0, -inf, 10000)
PL_TEST_INTERVAL2 (SV_NAME_F2 (hypot), -0, -inf, 0, inf, 10000)
PL_TEST_INTERVAL2 (SV_NAME_F2 (hypot), -0, -inf, -0, -inf, 10000)
