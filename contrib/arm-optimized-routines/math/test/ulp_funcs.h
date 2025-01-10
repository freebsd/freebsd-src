/*
 * Function entries for ulp.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/* clang-format off */
 F (sincosf_sinf, sincosf_sinf, sincos_sin, sincos_mpfr_sin, 1, 1, f1, 0)
 F (sincosf_cosf, sincosf_cosf, sincos_cos, sincos_mpfr_cos, 1, 1, f1, 0)
 F2 (pow)
 D2 (pow)
#if __aarch64__ && __linux__
 F (_ZGVnN4v_expf_1u, Z_expf_1u, exp, mpfr_exp, 1, 1, f1, 1)
 F (_ZGVnN4v_exp2f_1u, Z_exp2f_1u, exp2, mpfr_exp2, 1, 1, f1, 1)
 F (_ZGVnN4vv_powf, Z_powf, pow, mpfr_pow, 2, 1, f2, 1)
 F (_ZGVnN2vv_pow, Z_pow, powl, mpfr_pow, 2, 0, d2, 1)
 F (_ZGVnN4v_sincosf_sin, v_sincosf_sin, sin, mpfr_sin, 1, 1, f1, 0)
 F (_ZGVnN4v_sincosf_cos, v_sincosf_cos, cos, mpfr_cos, 1, 1, f1, 0)
 F (_ZGVnN4v_cexpif_sin, v_cexpif_sin, sin, mpfr_sin, 1, 1, f1, 0)
 F (_ZGVnN4v_cexpif_cos, v_cexpif_cos, cos, mpfr_cos, 1, 1, f1, 0)
 F (_ZGVnN4vl4_modff_frac, v_modff_frac, modf_frac, modf_mpfr_frac, 1, 1, f1, 0)
 F (_ZGVnN4vl4_modff_int, v_modff_int, modf_int, modf_mpfr_int, 1, 1, f1, 0)
 F (_ZGVnN2v_sincos_sin, v_sincos_sin, sinl, mpfr_sin, 1, 0, d1, 0)
 F (_ZGVnN2v_sincos_cos, v_sincos_cos, cosl, mpfr_cos, 1, 0, d1, 0)
 F (_ZGVnN2v_cexpi_sin, v_cexpi_sin, sinl, mpfr_sin, 1, 0, d1, 0)
 F (_ZGVnN2v_cexpi_cos, v_cexpi_cos, cosl, mpfr_cos, 1, 0, d1, 0)
 F (_ZGVnN2vl8_modf_frac, v_modf_frac, modfl_frac, modf_mpfr_frac, 1, 0, d1, 0)
 F (_ZGVnN2vl8_modf_int, v_modf_int, modfl_int, modf_mpfr_int, 1, 0, d1, 0)
#endif

#if WANT_SVE_TESTS
SVF (_ZGVsMxv_sincosf_sin, sv_sincosf_sin, sin, mpfr_sin, 1, 1, f1, 0)
SVF (_ZGVsMxv_sincosf_cos, sv_sincosf_cos, cos, mpfr_cos, 1, 1, f1, 0)
SVF (_ZGVsMxv_cexpif_sin, sv_cexpif_sin, sin, mpfr_sin, 1, 1, f1, 0)
SVF (_ZGVsMxv_cexpif_cos, sv_cexpif_cos, cos, mpfr_cos, 1, 1, f1, 0)
SVF (_ZGVsMxvl4_modff_frac, sv_modff_frac, modf_frac, modf_mpfr_frac, 1, 1, f1, 0)
SVF (_ZGVsMxvl4_modff_int, sv_modff_int, modf_int, modf_mpfr_int, 1, 1, f1, 0)
SVF (_ZGVsMxv_sincos_sin, sv_sincos_sin, sinl, mpfr_sin, 1, 0, d1, 0)
SVF (_ZGVsMxv_sincos_cos, sv_sincos_cos, cosl, mpfr_cos, 1, 0, d1, 0)
SVF (_ZGVsMxv_cexpi_sin, sv_cexpi_sin, sinl, mpfr_sin, 1, 0, d1, 0)
SVF (_ZGVsMxv_cexpi_cos, sv_cexpi_cos, cosl, mpfr_cos, 1, 0, d1, 0)
SVF (_ZGVsMxvl8_modf_frac, sv_modf_frac, modfl_frac, modf_mpfr_frac, 1, 0, d1, 0)
SVF (_ZGVsMxvl8_modf_int, sv_modf_int, modfl_int, modf_mpfr_int, 1, 0, d1, 0)
#endif

#if WANT_EXPERIMENTAL_MATH
 F (arm_math_erff, arm_math_erff, erf, mpfr_erf, 1, 1, f1, 0)
 F (arm_math_erf,  arm_math_erf,  erfl, mpfr_erf, 1, 0, d1, 0)
#endif

#if WANT_TRIGPI_TESTS
 F (arm_math_cospif, arm_math_cospif, arm_math_cospi, mpfr_cospi, 1, 1, f1, 0)
 F (arm_math_cospi,  arm_math_cospi,  arm_math_cospil, mpfr_cospi, 1, 0, d1, 0)
 F (arm_math_sinpif, arm_math_sinpif, arm_math_sinpi, mpfr_sinpi, 1, 1, f1, 0)
 F (arm_math_sinpi,  arm_math_sinpi,  arm_math_sinpil, mpfr_sinpi, 1, 0, d1, 0)
 F (arm_math_tanpif, arm_math_tanpif, arm_math_tanpi, mpfr_tanpi, 1, 1, f1, 0)
 F (arm_math_tanpi,  arm_math_tanpi,  arm_math_tanpil, mpfr_tanpi, 1, 0, d1, 0)
 F (arm_math_sincospif_sin, arm_math_sincospif_sin, arm_math_sinpi, mpfr_sinpi, 1, 1, f1, 0)
 F (arm_math_sincospif_cos, arm_math_sincospif_cos, arm_math_cospi, mpfr_cospi, 1, 1, f1, 0)
 F (arm_math_sincospi_sin, arm_math_sincospi_sin, arm_math_sinpil, mpfr_sinpi, 1, 0, d1, 0)
 F (arm_math_sincospi_cos, arm_math_sincospi_cos, arm_math_cospil, mpfr_cospi, 1, 0, d1, 0)
# if __aarch64__ && __linux__
 F (_ZGVnN4v_cospif, Z_cospif, arm_math_cospi,  mpfr_cospi, 1, 1, f1, 0)
 F (_ZGVnN2v_cospi,  Z_cospi,  arm_math_cospil, mpfr_cospi, 1, 0, d1, 0)
 F (_ZGVnN4v_sinpif, Z_sinpif, arm_math_sinpi,  mpfr_sinpi, 1, 1, f1, 0)
 F (_ZGVnN2v_sinpi,  Z_sinpi,  arm_math_sinpil, mpfr_sinpi, 1, 0, d1, 0)
 F (_ZGVnN4v_tanpif, Z_tanpif, arm_math_tanpi,  mpfr_tanpi, 1, 1, f1, 0)
 F (_ZGVnN2v_tanpi,  Z_tanpi,  arm_math_tanpil, mpfr_tanpi, 1, 0, d1, 0)
 F (_ZGVnN4v_sincospif_sin, v_sincospif_sin, arm_math_sinpi, mpfr_sinpi, 1, 1, f1, 0)
 F (_ZGVnN4v_sincospif_cos, v_sincospif_cos, arm_math_cospi, mpfr_cospi, 1, 1, f1, 0)
 F (_ZGVnN2v_sincospi_sin, v_sincospi_sin, arm_math_sinpil, mpfr_sinpi, 1, 0, d1, 0)
 F (_ZGVnN2v_sincospi_cos, v_sincospi_cos, arm_math_cospil, mpfr_cospi, 1, 0, d1, 0)
# endif
# if WANT_SVE_TESTS
 SVF (_ZGVsMxv_cospif, Z_sv_cospif, arm_math_cospi,  mpfr_cospi, 1, 1, f1, 0)
 SVF (_ZGVsMxv_cospi,  Z_sv_cospi,  arm_math_cospil, mpfr_cospi, 1, 0, d1, 0)
 SVF (_ZGVsMxv_sinpif, Z_sv_sinpif, arm_math_sinpi,  mpfr_sinpi, 1, 1, f1, 0)
 SVF (_ZGVsMxv_sinpi,  Z_sv_sinpi,  arm_math_sinpil, mpfr_sinpi, 1, 0, d1, 0)
 SVF (_ZGVsMxv_tanpif, Z_sv_tanpif, arm_math_tanpi,  mpfr_tanpi, 1, 1, f1, 0)
 SVF (_ZGVsMxv_tanpi,  Z_sv_tanpi,  arm_math_tanpil, mpfr_tanpi, 1, 0, d1, 0)
 SVF (_ZGVsMxvl4l4_sincospif_sin, sv_sincospif_sin, arm_math_sinpi, mpfr_sinpi, 1, 1, f1, 0)
 SVF (_ZGVsMxvl4l4_sincospif_cos, sv_sincospif_cos, arm_math_cospi, mpfr_cospi, 1, 1, f1, 0)
 SVF (_ZGVsMxvl8l8_sincospi_sin, sv_sincospi_sin, arm_math_sinpil, mpfr_sinpi, 1, 0, d1, 0)
 SVF (_ZGVsMxvl8l8_sincospi_cos, sv_sincospi_cos, arm_math_cospil, mpfr_cospi, 1, 0, d1, 0)
#  if WANT_EXPERIMENTAL_MATH
SVF (_ZGVsMxvv_powk, Z_sv_powk, ref_powi, mpfr_powi, 2, 0, d2, 0)
SVF (_ZGVsMxvv_powi, Z_sv_powi, ref_powif, mpfr_powi, 2, 1, f2, 0)
#  endif
# endif
#endif

 /* clang-format on */

#define _ZSF1(f) F1 (f)
#define _ZSF2(f) F2 (f)
#define _ZSD1(f) D1 (f)
#define _ZSD2(f) D2 (f)

#define _ZVF1(f) ZVNF1 (f)
#define _ZVD1(f) ZVND1 (f)
#define _ZVF2(f) ZVNF2 (f)
#define _ZVD2(f) ZVND2 (f)

#define _ZSVF1(f) ZSVF1 (f)
#define _ZSVF2(f) ZSVF2 (f)
#define _ZSVD1(f) ZSVD1 (f)
#define _ZSVD2(f) ZSVD2 (f)

#include "test/ulp_funcs_gen.h"
