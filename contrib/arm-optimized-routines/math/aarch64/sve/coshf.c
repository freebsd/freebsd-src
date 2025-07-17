/*
 * Single-precision SVE cosh(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_expf_inline.h"

static const struct data
{
  struct sv_expf_data expf_consts;
  float special_bound;
} data = {
  .expf_consts = SV_EXPF_DATA,
  /* 0x1.5a92d8p+6: expf overflows above this, so have to use special case.  */
  .special_bound = 0x1.5a92d8p+6,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t half_e, svfloat32_t half_over_e,
	      svbool_t pg)
{
  return sv_call_f32 (coshf, x, svadd_x (svptrue_b32 (), half_e, half_over_e),
		      pg);
}

/* Single-precision vector cosh, using vector expf.
   Maximum error is 2.77 ULP:
   _ZGVsMxv_coshf(-0x1.5b38f4p+1) got 0x1.e45946p+2
				 want 0x1.e4594cp+2.  */
svfloat32_t SV_NAME_F1 (cosh) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svbool_t special = svacge (pg, x, d->special_bound);

  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.
     Note that x is passed to exp here, rather than |x|. This is to avoid using
     destructive unary ABS for better register usage. However it means the
     routine is not exactly symmetrical, as the exp helper is slightly less
     accurate in the negative range.  */
  svfloat32_t e = expf_inline (x, pg, &d->expf_consts);
  svfloat32_t half_e = svmul_x (svptrue_b32 (), e, 0.5);
  svfloat32_t half_over_e = svdivr_x (pg, e, 0.5);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, half_e, half_over_e, special);

  return svadd_x (svptrue_b32 (), half_e, half_over_e);
}

TEST_SIG (SV, F, 1, cosh, -10.0, 10.0)
TEST_ULP (SV_NAME_F1 (cosh), 2.28)
TEST_DISABLE_FENV (SV_NAME_F1 (cosh))
TEST_SYM_INTERVAL (SV_NAME_F1 (cosh), 0, 0x1p-63, 100)
TEST_SYM_INTERVAL (SV_NAME_F1 (cosh), 0, 0x1.5a92d8p+6, 80000)
TEST_SYM_INTERVAL (SV_NAME_F1 (cosh), 0x1.5a92d8p+6, inf, 2000)
CLOSE_SVE_ATTR
