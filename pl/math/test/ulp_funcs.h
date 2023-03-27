/*
 * Function entries for ulp.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifdef __vpcs

#define _ZVF1(f) SF1 (f) VF1 (f) ZVNF1 (f)
#define _ZVD1(f) SD1 (f) VD1 (f) ZVND1 (f)
#define _ZVF2(f) SF2 (f) VF2 (f) ZVNF2 (f)
#define _ZVD2(f) SD2 (f) VD2 (f) ZVND2 (f)

#elif __aarch64

#define _ZVF1(f) SF1 (f) VF1 (f)
#define _ZVD1(f) SD1 (f) VD1 (f)
#define _ZVF2(f) SF2 (f) VF2 (f)
#define _ZVD2(f) SD2 (f) VD2 (f)

#elif WANT_VMATH

#define _ZVF1(f) SF1 (f)
#define _ZVD1(f) SD1 (f)
#define _ZVF2(f) SF2 (f)
#define _ZVD2(f) SD2 (f)

#else

#define _ZVF1(f)
#define _ZVD1(f)
#define _ZVF2(f)
#define _ZVD2(f)

#endif

#if WANT_SVE_MATH

#define _ZSVF1(f) SVF1 (f) ZSVF1 (f)
#define _ZSVF2(f) SVF2 (f) ZSVF2 (f)
#define _ZSVD1(f) SVD1 (f) ZSVD1 (f)
#define _ZSVD2(f) SVD2 (f) ZSVD2 (f)

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

#if WANT_SVE_MATH
F (__sv_powi, sv_powi, ref_powi, mpfr_powi, 2, 0, d2, 0)
F (_ZGVsMxvv_powk, Z_sv_powk, ref_powi, mpfr_powi, 2, 0, d2, 0)
F (__sv_powif, sv_powif, ref_powif, mpfr_powi, 2, 1, f2, 0)
F (_ZGVsMxvv_powi, Z_sv_powi, ref_powif, mpfr_powi, 2, 1, f2, 0)
#endif
