/*
 * Single-precision SVE powi(x, n) function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#if SV_SUPPORTED

/* Optimized single-precision vector powi (float base, integer power).
   powi is developed for environments in which accuracy is of much less
   importance than performance, hence we provide no estimate for worst-case
   error.  */
svfloat32_t
__sv_powif_x (svfloat32_t as, svint32_t ns, svbool_t p)
{
  /* Compute powi by successive squaring, right to left.  */
  svfloat32_t acc = svdup_n_f32 (1.f);
  svbool_t want_recip = svcmplt_n_s32 (p, ns, 0);
  svuint32_t ns_abs = svreinterpret_u32_s32 (svabs_s32_x (p, ns));

  /* We use a max to avoid needing to check whether any lane != 0 on each
     iteration.  */
  uint32_t max_n = svmaxv_u32 (p, ns_abs);

  svfloat32_t c = as;
  /* Successively square c, and use merging predication (_m) to determine
     whether or not to perform the multiplication or keep the previous
     iteration.  */
  while (true)
    {
      svbool_t px = svcmpeq_n_u32 (p, svand_n_u32_x (p, ns_abs, 1), 1);
      acc = svmul_f32_m (px, acc, c);
      max_n >>= 1;
      if (max_n == 0)
	break;

      ns_abs = svlsr_n_u32_x (p, ns_abs, 1);
      c = svmul_f32_x (p, c, c);
    }

  /* Negative powers are handled by computing the abs(n) version and then
     taking the reciprocal.  */
  if (svptest_any (want_recip, want_recip))
    acc = svdivr_n_f32_m (want_recip, acc, 1.0f);

  return acc;
}

/* Note no trailing f for ZGV... name - 64-bit integer version is powk.  */
strong_alias (__sv_powif_x, _ZGVsMxvv_powi)

#endif // SV_SUPPORTED
