/*
 * Function entries for ulp.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if defined(__vpcs) && __aarch64__

#define _ZVF1(f) ZVF1 (f)
#define _ZVD1(f) ZVD1 (f)
#define _ZVF2(f) ZVF2 (f)
#define _ZVD2(f) ZVD2 (f)

#else

#define _ZVF1(f)
#define _ZVD1(f)
#define _ZVF2(f)
#define _ZVD2(f)

#endif

#if WANT_SVE_MATH

#define _ZSVF1(f) ZSVF1 (f)
#define _ZSVF2(f) ZSVF2 (f)
#define _ZSVD1(f) ZSVD1 (f)
#define _ZSVD2(f) ZSVD2 (f)

#else

#define _ZSVF1(f)
#define _ZSVF2(f)
#define _ZSVD1(f)
#define _ZSVD2(f)

#endif

#define _ZSF1(f) F1 (f)
#define _ZSF2(f) F2 (f)
#define _ZSD1(f) D1 (f)
#define _ZSD2(f) D2 (f)

#include "ulp_funcs_gen.h"

F (_ZGVnN4v_sincosf_sin, v_sincosf_sin, sin, mpfr_sin, 1, 1, f1, 0)
F (_ZGVnN4v_sincosf_cos, v_sincosf_cos, cos, mpfr_cos, 1, 1, f1, 0)
F (_ZGVnN4v_cexpif_sin, v_cexpif_sin, sin, mpfr_sin, 1, 1, f1, 0)
F (_ZGVnN4v_cexpif_cos, v_cexpif_cos, cos, mpfr_cos, 1, 1, f1, 0)

F (_ZGVnN2v_sincos_sin, v_sincos_sin, sinl, mpfr_sin, 1, 0, d1, 0)
F (_ZGVnN2v_sincos_cos, v_sincos_cos, cosl, mpfr_cos, 1, 0, d1, 0)
F (_ZGVnN2v_cexpi_sin, v_cexpi_sin, sinl, mpfr_sin, 1, 0, d1, 0)
F (_ZGVnN2v_cexpi_cos, v_cexpi_cos, cosl, mpfr_cos, 1, 0, d1, 0)

#if WANT_SVE_MATH
F (_ZGVsMxvv_powk, Z_sv_powk, ref_powi, mpfr_powi, 2, 0, d2, 0)
F (_ZGVsMxvv_powi, Z_sv_powi, ref_powif, mpfr_powi, 2, 1, f2, 0)

F (_ZGVsMxv_sincosf_sin, sv_sincosf_sin, sin, mpfr_sin, 1, 1, f1, 0)
F (_ZGVsMxv_sincosf_cos, sv_sincosf_cos, cos, mpfr_cos, 1, 1, f1, 0)
F (_ZGVsMxv_cexpif_sin, sv_cexpif_sin, sin, mpfr_sin, 1, 1, f1, 0)
F (_ZGVsMxv_cexpif_cos, sv_cexpif_cos, cos, mpfr_cos, 1, 1, f1, 0)

F (_ZGVsMxv_sincos_sin, sv_sincos_sin, sinl, mpfr_sin, 1, 0, d1, 0)
F (_ZGVsMxv_sincos_cos, sv_sincos_cos, cosl, mpfr_cos, 1, 0, d1, 0)
F (_ZGVsMxv_cexpi_sin, sv_cexpi_sin, sinl, mpfr_sin, 1, 0, d1, 0)
F (_ZGVsMxv_cexpi_cos, sv_cexpi_cos, cosl, mpfr_cos, 1, 0, d1, 0)
#endif
