/*
 * Function entries for mathbench.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
D (exp, -9.9, 9.9)
D (exp, 0.5, 1.0)
D (exp2, -9.9, 9.9)
D (log, 0.01, 11.1)
D (log, 0.999, 1.001)
D (log2, 0.01, 11.1)
D (log2, 0.999, 1.001)
{"pow", 'd', 0, 0.01, 11.1, {.d = xypow}},
D (xpow, 0.01, 11.1)
D (ypow, -9.9, 9.9)
D (erf, -6.0, 6.0)

F (expf, -9.9, 9.9)
F (exp2f, -9.9, 9.9)
F (logf, 0.01, 11.1)
F (log2f, 0.01, 11.1)
{"powf", 'f', 0, 0.01, 11.1, {.f = xypowf}},
F (xpowf, 0.01, 11.1)
F (ypowf, -9.9, 9.9)
{"sincosf", 'f', 0, 0.1, 0.7, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 0.8, 3.1, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, -3.1, 3.1, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 3.3, 33.3, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 100, 1000, {.f = sincosf_wrap}},
{"sincosf", 'f', 0, 1e6, 1e32, {.f = sincosf_wrap}},
F (sinf, 0.1, 0.7)
F (sinf, 0.8, 3.1)
F (sinf, -3.1, 3.1)
F (sinf, 3.3, 33.3)
F (sinf, 100, 1000)
F (sinf, 1e6, 1e32)
F (cosf, 0.1, 0.7)
F (cosf, 0.8, 3.1)
F (cosf, -3.1, 3.1)
F (cosf, 3.3, 33.3)
F (cosf, 100, 1000)
F (cosf, 1e6, 1e32)
F (erff, -4.0, 4.0)
#if WANT_VMATH
D (__s_sin, -3.1, 3.1)
D (__s_cos, -3.1, 3.1)
D (__s_exp, -9.9, 9.9)
D (__s_log, 0.01, 11.1)
{"__s_pow", 'd', 0, 0.01, 11.1, {.d = xy__s_pow}},
F (__s_expf, -9.9, 9.9)
F (__s_expf_1u, -9.9, 9.9)
F (__s_exp2f, -9.9, 9.9)
F (__s_exp2f_1u, -9.9, 9.9)
F (__s_logf, 0.01, 11.1)
{"__s_powf", 'f', 0, 0.01, 11.1, {.f = xy__s_powf}},
F (__s_sinf, -3.1, 3.1)
F (__s_cosf, -3.1, 3.1)
#if __aarch64__
VD (__v_sin, -3.1, 3.1)
VD (__v_cos, -3.1, 3.1)
VD (__v_exp, -9.9, 9.9)
VD (__v_log, 0.01, 11.1)
{"__v_pow", 'd', 'v', 0.01, 11.1, {.vd = xy__v_pow}},
VF (__v_expf, -9.9, 9.9)
VF (__v_expf_1u, -9.9, 9.9)
VF (__v_exp2f, -9.9, 9.9)
VF (__v_exp2f_1u, -9.9, 9.9)
VF (__v_logf, 0.01, 11.1)
{"__v_powf", 'f', 'v', 0.01, 11.1, {.vf = xy__v_powf}},
VF (__v_sinf, -3.1, 3.1)
VF (__v_cosf, -3.1, 3.1)
#ifdef __vpcs
VND (__vn_exp, -9.9, 9.9)
VND (_ZGVnN2v_exp, -9.9, 9.9)
VND (__vn_log, 0.01, 11.1)
VND (_ZGVnN2v_log, 0.01, 11.1)
{"__vn_pow", 'd', 'n', 0.01, 11.1, {.vnd = xy__vn_pow}},
{"_ZGVnN2vv_pow", 'd', 'n', 0.01, 11.1, {.vnd = xy_Z_pow}},
VND (__vn_sin, -3.1, 3.1)
VND (_ZGVnN2v_sin, -3.1, 3.1)
VND (__vn_cos, -3.1, 3.1)
VND (_ZGVnN2v_cos, -3.1, 3.1)
VNF (__vn_expf, -9.9, 9.9)
VNF (_ZGVnN4v_expf, -9.9, 9.9)
VNF (__vn_expf_1u, -9.9, 9.9)
VNF (__vn_exp2f, -9.9, 9.9)
VNF (_ZGVnN4v_exp2f, -9.9, 9.9)
VNF (__vn_exp2f_1u, -9.9, 9.9)
VNF (__vn_logf, 0.01, 11.1)
VNF (_ZGVnN4v_logf, 0.01, 11.1)
{"__vn_powf", 'f', 'n', 0.01, 11.1, {.vnf = xy__vn_powf}},
{"_ZGVnN4vv_powf", 'f', 'n', 0.01, 11.1, {.vnf = xy_Z_powf}},
VNF (__vn_sinf, -3.1, 3.1)
VNF (_ZGVnN4v_sinf, -3.1, 3.1)
VNF (__vn_cosf, -3.1, 3.1)
VNF (_ZGVnN4v_cosf, -3.1, 3.1)
#endif
#endif
#endif
