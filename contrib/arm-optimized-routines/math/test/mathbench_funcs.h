/*
 * Function entries for mathbench.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/* clang-format off */
D (exp, -9.9, 9.9)
D (exp, 0.5, 1.0)
D (exp10, -9.9, 9.9)
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
#ifdef __vpcs
VND (_ZGVnN2v_exp, -9.9, 9.9)
VND (_ZGVnN2v_log, 0.01, 11.1)
{"_ZGVnN2vv_pow", 'd', 'n', 0.01, 11.1, {.vnd = xy_Z_pow}},
VND (_ZGVnN2v_sin, -3.1, 3.1)
VND (_ZGVnN2v_cos, -3.1, 3.1)
VNF (_ZGVnN4v_expf, -9.9, 9.9)
VNF (_ZGVnN4v_expf_1u, -9.9, 9.9)
VNF (_ZGVnN4v_exp2f, -9.9, 9.9)
VNF (_ZGVnN4v_exp2f_1u, -9.9, 9.9)
VNF (_ZGVnN4v_logf, 0.01, 11.1)
{"_ZGVnN4vv_powf", 'f', 'n', 0.01, 11.1, {.vnf = xy_Z_powf}},
VNF (_ZGVnN4v_sinf, -3.1, 3.1)
VNF (_ZGVnN4v_cosf, -3.1, 3.1)
#endif
  /* clang-format on */
