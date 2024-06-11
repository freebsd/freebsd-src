// clang-format off
/*
 * Function entries for mathbench.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#define _ZSF1(fun, a, b) F(fun##f, a, b)
#define _ZSD1(f, a, b) D(f, a, b)

#if defined(__vpcs) && __aarch64__

#define _ZVF1(fun, a, b) VNF(_ZGVnN4v_##fun##f, a, b)
#define _ZVD1(f, a, b) VND(_ZGVnN2v_##f, a, b)

#else

#define _ZVF1(f, a, b)
#define _ZVD1(f, a, b)

#endif

#if WANT_SVE_MATH

#define _ZSVF1(fun, a, b) SVF(_ZGVsMxv_##fun##f, a, b)
#define _ZSVD1(f, a, b) SVD(_ZGVsMxv_##f, a, b)

#else

#define _ZSVF1(f, a, b)
#define _ZSVD1(f, a, b)

#endif

/* No auto-generated wrappers for binary functions - they have be
   manually defined in mathbench_wrappers.h. We have to define silent
   macros for them anyway as they will be emitted by PL_SIG.  */
#define _ZSF2(...)
#define _ZSD2(...)
#define _ZVF2(...)
#define _ZVD2(...)
#define _ZSVF2(...)
#define _ZSVD2(...)

#include "mathbench_funcs_gen.h"

/* PL_SIG only emits entries for unary functions, since if a function
   needs to be wrapped in mathbench there is no way for it to know the
   same of the wrapper. Add entries for binary functions, or any other
   exotic signatures that need wrapping, below.  */

{"atan2f", 'f', 0, -10.0, 10.0, {.f = atan2f_wrap}},
{"atan2",  'd', 0, -10.0, 10.0, {.d = atan2_wrap}},
{"powi",   'd', 0,  0.01, 11.1, {.d = powi_wrap}},

{"_ZGVnN4vv_atan2f", 'f', 'n', -10.0, 10.0, {.vnf = _Z_atan2f_wrap}},
{"_ZGVnN2vv_atan2",  'd', 'n', -10.0, 10.0, {.vnd = _Z_atan2_wrap}},
{"_ZGVnN4vv_hypotf", 'f', 'n', -10.0, 10.0, {.vnf = _Z_hypotf_wrap}},
{"_ZGVnN2vv_hypot",  'd', 'n', -10.0, 10.0, {.vnd = _Z_hypot_wrap}},
{"_ZGVnN2vv_pow",    'd', 'n', -10.0, 10.0, {.vnd = xy_Z_pow}},
{"x_ZGVnN2vv_pow",   'd', 'n', -10.0, 10.0, {.vnd = x_Z_pow}},
{"y_ZGVnN2vv_pow",   'd', 'n', -10.0, 10.0, {.vnd = y_Z_pow}},
{"_ZGVnN4vl4l4_sincosf", 'f', 'n', -3.1, 3.1, {.vnf = _Z_sincosf_wrap}},
{"_ZGVnN2vl8l8_sincos", 'd', 'n', -3.1, 3.1, {.vnd = _Z_sincos_wrap}},
{"_ZGVnN4v_cexpif", 'f', 'n', -3.1, 3.1, {.vnf = _Z_cexpif_wrap}},
{"_ZGVnN2v_cexpi", 'd', 'n', -3.1, 3.1, {.vnd = _Z_cexpi_wrap}},

#if WANT_SVE_MATH
{"_ZGVsMxvv_atan2f", 'f', 's', -10.0, 10.0, {.svf = _Z_sv_atan2f_wrap}},
{"_ZGVsMxvv_atan2",  'd', 's', -10.0, 10.0, {.svd = _Z_sv_atan2_wrap}},
{"_ZGVsMxvv_hypotf", 'f', 's', -10.0, 10.0, {.svf = _Z_sv_hypotf_wrap}},
{"_ZGVsMxvv_hypot",  'd', 's', -10.0, 10.0, {.svd = _Z_sv_hypot_wrap}},
{"_ZGVsMxvv_powi",   'f', 's', -10.0, 10.0, {.svf = _Z_sv_powi_wrap}},
{"_ZGVsMxvv_powk",   'd', 's', -10.0, 10.0, {.svd = _Z_sv_powk_wrap}},
{"_ZGVsMxvv_powf",   'f', 's', -10.0, 10.0, {.svf = xy_Z_sv_powf}},
{"x_ZGVsMxvv_powf",  'f', 's', -10.0, 10.0, {.svf = x_Z_sv_powf}},
{"y_ZGVsMxvv_powf",  'f', 's', -10.0, 10.0, {.svf = y_Z_sv_powf}},
{"_ZGVsMxvv_pow",    'd', 's', -10.0, 10.0, {.svd = xy_Z_sv_pow}},
{"x_ZGVsMxvv_pow",   'd', 's', -10.0, 10.0, {.svd = x_Z_sv_pow}},
{"y_ZGVsMxvv_pow",   'd', 's', -10.0, 10.0, {.svd = y_Z_sv_pow}},
{"_ZGVsMxvl4l4_sincosf", 'f', 's', -3.1, 3.1, {.svf = _Z_sv_sincosf_wrap}},
{"_ZGVsMxvl8l8_sincos", 'd', 's', -3.1, 3.1, {.svd = _Z_sv_sincos_wrap}},
{"_ZGVsMxv_cexpif", 'f', 's', -3.1, 3.1, {.svf = _Z_sv_cexpif_wrap}},
{"_ZGVsMxv_cexpi", 'd', 's', -3.1, 3.1, {.svd = _Z_sv_cexpi_wrap}},
#endif
    // clang-format on
