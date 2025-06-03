/*
 * Double-precision vector sinh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_expm1_inline.h"

static const struct data
{
  struct v_expm1_data d;
  uint64x2_t halff;
#if WANT_SIMD_EXCEPT
  uint64x2_t tiny_bound, thresh;
#else
  float64x2_t large_bound;
#endif
} data = {
  .d = V_EXPM1_DATA,
  .halff = V2 (0x3fe0000000000000),
#if WANT_SIMD_EXCEPT
  /* 2^-26, below which sinh(x) rounds to x.  */
  .tiny_bound = V2 (0x3e50000000000000),
  /* asuint(large_bound) - asuint(tiny_bound).  */
  .thresh = V2 (0x0230000000000000),
#else
  /* 2^9. expm1 helper overflows for large input.  */
  .large_bound = V2 (0x1p+9),
#endif
};

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x)
{
  return v_call_f64 (sinh, x, x, v_u64 (-1));
}

/* Approximation for vector double-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The greatest observed error is 2.52 ULP:
   _ZGVnN2v_sinh(-0x1.a098a2177a2b9p-2) got -0x1.ac2f05bb66fccp-2
				       want -0x1.ac2f05bb66fc9p-2.  */
float64x2_t VPCS_ATTR V_NAME_D1 (sinh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  float64x2_t halfsign = vreinterpretq_f64_u64 (
      vbslq_u64 (v_u64 (0x8000000000000000), ix, d->halff));

#if WANT_SIMD_EXCEPT
  uint64x2_t special = vcgeq_u64 (
      vsubq_u64 (vreinterpretq_u64_f64 (ax), d->tiny_bound), d->thresh);
#else
  uint64x2_t special = vcageq_f64 (x, d->large_bound);
#endif

  /* Fall back to scalar variant for all lanes if any of them are special.  */
  if (unlikely (v_any_u64 (special)))
    return special_case (x);

  /* Up to the point that expm1 overflows, we can use it to calculate sinh
     using a slight rearrangement of the definition of sinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  float64x2_t t = expm1_inline (ax, &d->d);
  t = vaddq_f64 (t, vdivq_f64 (t, vaddq_f64 (t, v_f64 (1.0))));
  return vmulq_f64 (t, halfsign);
}

TEST_SIG (V, D, 1, sinh, -10.0, 10.0)
TEST_ULP (V_NAME_D1 (sinh), 2.02)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (sinh), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_D1 (sinh), 0, 0x1p-26, 1000)
TEST_SYM_INTERVAL (V_NAME_D1 (sinh), 0x1p-26, 0x1p9, 500000)
TEST_SYM_INTERVAL (V_NAME_D1 (sinh), 0x1p9, inf, 1000)
