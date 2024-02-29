/*
 * Function entries for ulp.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/* clang-format off */
 F1 (sin)
 F1 (cos)
 F (sincosf_sinf, sincosf_sinf, sincos_sin, sincos_mpfr_sin, 1, 1, f1, 0)
 F (sincosf_cosf, sincosf_cosf, sincos_cos, sincos_mpfr_cos, 1, 1, f1, 0)
 F1 (exp)
 F1 (exp2)
 F1 (log)
 F1 (log2)
 F2 (pow)
 F1 (erf)
 D1 (exp)
 D1 (exp10)
 D1 (exp2)
 D1 (log)
 D1 (log2)
 D2 (pow)
 D1 (erf)
#ifdef __vpcs
 F (_ZGVnN4v_sinf, Z_sinf, sin, mpfr_sin, 1, 1, f1, 1)
 F (_ZGVnN4v_cosf, Z_cosf, cos, mpfr_cos, 1, 1, f1, 1)
 F (_ZGVnN4v_expf_1u, Z_expf_1u, exp, mpfr_exp, 1, 1, f1, 1)
 F (_ZGVnN4v_expf, Z_expf, exp, mpfr_exp, 1, 1, f1, 1)
 F (_ZGVnN4v_exp2f_1u, Z_exp2f_1u, exp2, mpfr_exp2, 1, 1, f1, 1)
 F (_ZGVnN4v_exp2f, Z_exp2f, exp2, mpfr_exp2, 1, 1, f1, 1)
 F (_ZGVnN4v_logf, Z_logf, log, mpfr_log, 1, 1, f1, 1)
 F (_ZGVnN4vv_powf, Z_powf, pow, mpfr_pow, 2, 1, f2, 1)
 F (_ZGVnN2v_sin, Z_sin, sinl, mpfr_sin, 1, 0, d1, 1)
 F (_ZGVnN2v_cos, Z_cos, cosl, mpfr_cos, 1, 0, d1, 1)
 F (_ZGVnN2v_exp, Z_exp, expl, mpfr_exp, 1, 0, d1, 1)
 F (_ZGVnN2v_log, Z_log, logl, mpfr_log, 1, 0, d1, 1)
 F (_ZGVnN2vv_pow, Z_pow, powl, mpfr_pow, 2, 0, d2, 1)
#endif
/* clang-format on */
