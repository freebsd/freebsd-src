/*
 * Function entries for mathbench.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/* clang-format off */
{"pow", 'd', 0, 0.01, 11.1, {.d = xypow}},
D (xpow, 0.01, 11.1)
D (ypow, -9.9, 9.9)
{"powf", 'f', 0, 0.01, 11.1, {.f = xypowf}},
F (xpowf, 0.01, 11.1)
F (ypowf, -9.9, 9.9)
{"sincosf", 'f', 0, 0.1, 0.7, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 0.8, 3.1, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, -3.1, 3.1, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 3.3, 33.3, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 100, 1000, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 1e6, 1e32, {.f = sincosf_wrap}},
#if WANT_TRIGPI_TESTS
F (arm_math_cospif, -0.9, 0.9)
D (arm_math_cospi, -0.9, 0.9)
F (arm_math_sinpif, -0.9, 0.9)
D (arm_math_sinpi, -0.9, 0.9)
F (arm_math_tanpif, -0.9, 0.9)
D (arm_math_tanpi, -0.9, 0.9)
{"sincospif", 'f', 0, -0.9, 0.9, {.f = sincospif_wrap}},
{"sincospi", 'd', 0, -0.9, 0.9, {.d = sincospi_wrap}},
#endif
#if WANT_EXPERIMENTAL_MATH
D (arm_math_erf, -6.0, 6.0)
F (arm_math_erff, -4.0, 4.0)
{"atan2f", 'f', 0, -10.0, 10.0, {.f = atan2f_wrap}},
{"atan2",  'd', 0, -10.0, 10.0, {.d = atan2_wrap}},
{"powi",   'd', 0,  0.01, 11.1, {.d = powi_wrap}},
#endif
#if __aarch64__ && __linux__
{"_ZGVnN4vv_atan2f", 'f', 'n', -10.0, 10.0, {.vnf = _Z_atan2f_wrap}},
{"_ZGVnN2vv_atan2",  'd', 'n', -10.0, 10.0, {.vnd = _Z_atan2_wrap}},
{"_ZGVnN4vv_hypotf", 'f', 'n', -10.0, 10.0, {.vnf = _Z_hypotf_wrap}},
{"_ZGVnN2vv_hypot",  'd', 'n', -10.0, 10.0, {.vnd = _Z_hypot_wrap}},
{"_ZGVnN2vv_pow",    'd', 'n', -10.0, 10.0, {.vnd = xy_Z_pow}},
{"x_ZGVnN2vv_pow",   'd', 'n', -10.0, 10.0, {.vnd = x_Z_pow}},
{"y_ZGVnN2vv_pow",   'd', 'n', -10.0, 10.0, {.vnd = y_Z_pow}},
{"_ZGVnN4vv_powf",  'f', 'n',   0.01, 11.1, {.vnf = xy_Z_powf}},
{"x_ZGVnN4vv_powf", 'f', 'n',   0.01, 11.1, {.vnf = x_Z_powf}},
{"y_ZGVnN4vv_powf", 'f', 'n', -10.0,  10.0, {.vnf = y_Z_powf}},
{"_ZGVnN4vl4_modff", 'f', 'n', -10.0, 10.0, {.vnf = _Z_modff_wrap}},
{"_ZGVnN2vl8_modf",  'd', 'n', -10.0, 10.0, {.vnd = _Z_modf_wrap}},
{"_ZGVnN4vl4l4_sincosf", 'f', 'n', -3.1, 3.1, {.vnf = _Z_sincosf_wrap}},
{"_ZGVnN2vl8l8_sincos", 'd', 'n', -3.1, 3.1, {.vnd = _Z_sincos_wrap}},
{"_ZGVnN4v_cexpif", 'f', 'n', -3.1, 3.1, {.vnf = _Z_cexpif_wrap}},
{"_ZGVnN2v_cexpi", 'd', 'n', -3.1, 3.1, {.vnd = _Z_cexpi_wrap}},
VNF (_ZGVnN4v_expf_1u, -9.9, 9.9)
VNF (_ZGVnN4v_exp2f_1u, -9.9, 9.9)
# if WANT_TRIGPI_TESTS
VNF (_ZGVnN4v_cospif, -0.9, 0.9)
VND (_ZGVnN2v_cospi, -0.9, 0.9)
VNF (_ZGVnN4v_sinpif, -0.9, 0.9)
VND (_ZGVnN2v_sinpi, -0.9, 0.9)
VNF (_ZGVnN4v_tanpif, -0.9, 0.9)
VND (_ZGVnN2v_tanpi, -0.9, 0.9)
{"_ZGVnN4vl4l4_sincospif", 'f', 'n', -0.9, 0.9, {.vnf = _Z_sincospif_wrap}},
{"_ZGVnN2vl8l8_sincospi", 'd', 'n', -0.9, 0.9, {.vnd = _Z_sincospi_wrap}},
# endif
#endif

#if WANT_SVE_TESTS
{ "_ZGVsMxvv_atan2f", 'f', 's', -10.0, 10.0, { .svf = _Z_sv_atan2f_wrap } },
{ "_ZGVsMxvv_atan2", 'd', 's', -10.0, 10.0, { .svd = _Z_sv_atan2_wrap } },
{ "_ZGVsMxvv_hypotf", 'f', 's', -10.0, 10.0, { .svf = _Z_sv_hypotf_wrap } },
{ "_ZGVsMxvv_hypot", 'd', 's', -10.0, 10.0, { .svd = _Z_sv_hypot_wrap } },
{"_ZGVsMxvv_powf",   'f', 's', -10.0, 10.0, {.svf = xy_Z_sv_powf}},
{"x_ZGVsMxvv_powf",  'f', 's', -10.0, 10.0, {.svf = x_Z_sv_powf}},
{"y_ZGVsMxvv_powf",  'f', 's', -10.0, 10.0, {.svf = y_Z_sv_powf}},
{"_ZGVsMxvv_pow",    'd', 's', -10.0, 10.0, {.svd = xy_Z_sv_pow}},
{"x_ZGVsMxvv_pow",   'd', 's', -10.0, 10.0, {.svd = x_Z_sv_pow}},
{"y_ZGVsMxvv_pow",   'd', 's', -10.0, 10.0, {.svd = y_Z_sv_pow}},
{"_ZGVsMxvl4_modff", 'f', 's', -10.0, 10.0, {.svf = _Z_sv_modff_wrap}},
{"_ZGVsMxvl8_modf",  'd', 's', -10.0, 10.0, {.svd = _Z_sv_modf_wrap}},
{"_ZGVsMxvl4l4_sincosf", 'f', 's', -3.1, 3.1, {.svf = _Z_sv_sincosf_wrap}},
{"_ZGVsMxvl8l8_sincos", 'd', 's', -3.1, 3.1, {.svd = _Z_sv_sincos_wrap}},
{"_ZGVsMxv_cexpif", 'f', 's', -3.1, 3.1, {.svf = _Z_sv_cexpif_wrap}},
{"_ZGVsMxv_cexpi", 'd', 's', -3.1, 3.1, {.svd = _Z_sv_cexpi_wrap}},
# if WANT_TRIGPI_TESTS
SVF (_ZGVsMxv_cospif, -0.9, 0.9)
SVD (_ZGVsMxv_cospi, -0.9, 0.9)
SVF (_ZGVsMxv_sinpif, -0.9, 0.9)
SVD (_ZGVsMxv_sinpi, -0.9, 0.9)
SVF (_ZGVsMxv_tanpif, -0.9, 0.9)
SVD (_ZGVsMxv_tanpi, -0.9, 0.9)
{"_ZGVsMxvl4l4_sincospif", 'f', 's', -0.9, 0.9, {.svf = _Z_sv_sincospif_wrap}},
{"_ZGVsMxvl8l8_sincospi", 'd', 's', -0.9, 0.9, {.svd = _Z_sv_sincospi_wrap}},
# endif
# if WANT_EXPERIMENTAL_MATH
{"_ZGVsMxvv_powi",   'f', 's', -10.0, 10.0, {.svf = _Z_sv_powi_wrap}},
{"_ZGVsMxvv_powk",   'd', 's', -10.0, 10.0, {.svd = _Z_sv_powk_wrap}},
# endif
#endif
    /* clang-format on */

#define _ZSF1(fun, a, b) F (fun##f, a, b)
#define _ZSD1(f, a, b) D (f, a, b)

#define _ZVF1(fun, a, b) VNF (_ZGVnN4v_##fun##f, a, b)
#define _ZVD1(f, a, b) VND (_ZGVnN2v_##f, a, b)

#define _ZSVF1(fun, a, b) SVF (_ZGVsMxv_##fun##f, a, b)
#define _ZSVD1(f, a, b) SVD (_ZGVsMxv_##f, a, b)

/* No auto-generated wrappers for binary functions - they have be
   manually defined in mathbench_wrappers.h. We have to define silent
   macros for them anyway as they will be emitted by TEST_SIG.  */
#define _ZSF2(...)
#define _ZSD2(...)
#define _ZVF2(...)
#define _ZVD2(...)
#define _ZSVF2(...)
#define _ZSVD2(...)

#include "test/mathbench_funcs_gen.h"
