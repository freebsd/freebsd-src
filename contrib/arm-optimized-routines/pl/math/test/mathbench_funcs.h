// clang-format off
/*
 * Function entries for mathbench.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#define _ZSF1(fun, a, b) F(fun##f, a, b)
#define _ZSD1(f, a, b) D(f, a, b)

#ifdef __vpcs

#define _ZVF1(fun, a, b) F(__s_##fun##f, a, b) VF(__v_##fun##f, a, b) VNF(__vn_##fun##f, a, b) VNF(_ZGVnN4v_##fun##f, a, b)
#define _ZVD1(f, a, b) D(__s_##f, a, b) VD(__v_##f, a, b) VND(__vn_##f, a, b) VND(_ZGVnN2v_##f, a, b)

#elif __aarch64__

#define _ZVF1(fun, a, b) F(__s_##fun##f, a, b) VF(__v_##fun##f, a, b)
#define _ZVD1(f, a, b) D(__s_##f, a, b) VD(__v_##f, a, b)

#elif WANT_VMATH

#define _ZVF1(fun, a, b) F(__s_##fun##f, a, b)
#define _ZVD1(f, a, b) D(__s_##f, a, b)

#else

#define _ZVF1(f, a, b)
#define _ZVD1(f, a, b)

#endif

#if WANT_SVE_MATH

#define _ZSVF1(fun, a, b) SVF(__sv_##fun##f_x, a, b) SVF(_ZGVsMxv_##fun##f, a, b)
#define _ZSVD1(f, a, b) SVD(__sv_##f##_x, a, b) SVD(_ZGVsMxv_##f, a, b)

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

{"__s_atan2f",       'f', 0,   -10.0, 10.0, {.f = __s_atan2f_wrap}},
{"__s_atan2",        'd', 0,   -10.0, 10.0, {.d = __s_atan2_wrap}},
{"__v_atan2f",       'f', 'v', -10.0, 10.0, {.vf = __v_atan2f_wrap}},
{"__v_atan2",        'd', 'v', -10.0, 10.0, {.vd = __v_atan2_wrap}},
{"__vn_atan2f",      'f', 'n', -10.0, 10.0, {.vnf = __vn_atan2f_wrap}},
{"_ZGVnN4vv_atan2f", 'f', 'n', -10.0, 10.0, {.vnf = _Z_atan2f_wrap}},
{"__vn_atan2",       'd', 'n', -10.0, 10.0, {.vnd = __vn_atan2_wrap}},
{"_ZGVnN2vv_atan2",  'd', 'n', -10.0, 10.0, {.vnd = _Z_atan2_wrap}},

#if WANT_SVE_MATH
{"__sv_atan2f_x",    'f', 's', -10.0, 10.0, {.svf = __sv_atan2f_wrap}},
{"_ZGVsMxvv_atan2f", 'f', 's', -10.0, 10.0, {.svf = _Z_sv_atan2f_wrap}},
{"__sv_atan2_x",     'd', 's', -10.0, 10.0, {.svd = __sv_atan2_wrap}},
{"_ZGVsM2vv_atan2",  'd', 's', -10.0, 10.0, {.svd = _Z_sv_atan2_wrap}},
{"__sv_powif_x",     'f', 's', -10.0, 10.0, {.svf = __sv_powif_wrap}},
{"_ZGVsMxvv_powi",   'f', 's', -10.0, 10.0, {.svf = _Z_sv_powi_wrap}},
{"__sv_powi_x",      'd', 's', -10.0, 10.0, {.svd = __sv_powi_wrap}},
{"_ZGVsMxvv_powk",   'd', 's', -10.0, 10.0, {.svd = _Z_sv_powk_wrap}},
#endif
  // clang-format on
