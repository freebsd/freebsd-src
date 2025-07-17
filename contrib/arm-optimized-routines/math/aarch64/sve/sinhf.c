/*
 * Single-precision SVE sinh(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_expm1f_inline.h"

static const struct data
{
  struct sv_expm1f_data expm1f_consts;
  uint32_t halff, large_bound;
} data = {
  .expm1f_consts = SV_EXPM1F_DATA,
  .halff = 0x3f000000,
  /* 0x1.61814ep+6, above which expm1f helper overflows.  */
  .large_bound = 0x42b0c0a7,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t pg)
{
  return sv_call_f32 (sinhf, x, y, pg);
}

/* Approximation for SVE single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The maximum error is 2.26 ULP:
   _ZGVsMxv_sinhf (0x1.e34a9ep-4) got 0x1.e469ep-4
				 want 0x1.e469e4p-4.  */
svfloat32_t SV_NAME_F1 (sinh) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t sign
      = sveor_x (pg, svreinterpret_u32 (x), svreinterpret_u32 (ax));
  svfloat32_t halfsign = svreinterpret_f32 (svorr_x (pg, sign, d->halff));

  svbool_t special = svcmpge (pg, svreinterpret_u32 (ax), d->large_bound);

  /* Up to the point that expm1f overflows, we can use it to calculate sinhf
   using a slight rearrangement of the definition of asinh. This allows us to
   retain acceptable accuracy for very small inputs.  */
  svfloat32_t t = expm1f_inline (ax, pg, &d->expm1f_consts);
  t = svadd_x (pg, t, svdiv_x (pg, t, svadd_x (pg, t, 1.0)));

  /* Fall back to the scalar variant for any lanes which would cause
     expm1f to overflow.  */
  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svmul_x (pg, t, halfsign), special);

  return svmul_x (svptrue_b32 (), t, halfsign);
}

TEST_SIG (SV, F, 1, sinh, -10.0, 10.0)
TEST_ULP (SV_NAME_F1 (sinh), 1.76)
TEST_DISABLE_FENV (SV_NAME_F1 (sinh))
TEST_SYM_INTERVAL (SV_NAME_F1 (sinh), 0, 0x1.6a09e8p-32, 1000)
TEST_SYM_INTERVAL (SV_NAME_F1 (sinh), 0x1.6a09e8p-32, 0x42b0c0a7, 100000)
TEST_SYM_INTERVAL (SV_NAME_F1 (sinh), 0x42b0c0a7, inf, 1000)
CLOSE_SVE_ATTR
