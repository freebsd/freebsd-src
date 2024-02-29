/*
 * Single-precision SVE cosh(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#include "sv_expf_inline.h"

static const struct data
{
  struct sv_expf_data expf_consts;
  uint32_t special_bound;
} data = {
  .expf_consts = SV_EXPF_DATA,
  /* 0x1.5a92d8p+6: expf overflows above this, so have to use special case.  */
  .special_bound = 0x42ad496c,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t pg)
{
  return sv_call_f32 (coshf, x, y, pg);
}

/* Single-precision vector cosh, using vector expf.
   Maximum error is 1.89 ULP:
   _ZGVsMxv_coshf (-0x1.65898cp+6) got 0x1.f00aep+127
				  want 0x1.f00adcp+127.  */
svfloat32_t SV_NAME_F1 (cosh) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat32_t ax = svabs_x (pg, x);
  svbool_t special = svcmpge (pg, svreinterpret_u32 (ax), d->special_bound);

  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.  */
  svfloat32_t t = expf_inline (ax, pg, &d->expf_consts);
  svfloat32_t half_t = svmul_x (pg, t, 0.5);
  svfloat32_t half_over_t = svdivr_x (pg, t, 0.5);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svadd_x (pg, half_t, half_over_t), special);

  return svadd_x (pg, half_t, half_over_t);
}

PL_SIG (SV, F, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_F1 (cosh), 1.39)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cosh), 0, 0x1p-63, 100)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cosh), 0, 0x1.5a92d8p+6, 80000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cosh), 0x1.5a92d8p+6, inf, 2000)
