/*
 * Function wrappers for ulp.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* clang-format off */

/* Wrappers for sincos.  */
static float sincosf_sinf(float x) {(void)cosf(x); return sinf(x);}
static float sincosf_cosf(float x) {(void)sinf(x); return cosf(x);}
static double sincos_sin(double x) {(void)cos(x); return sin(x);}
static double sincos_cos(double x) {(void)sin(x); return cos(x);}
#if USE_MPFR
static int sincos_mpfr_sin(mpfr_t y, const mpfr_t x, mpfr_rnd_t r) { mpfr_cos(y,x,r); return mpfr_sin(y,x,r); }
static int sincos_mpfr_cos(mpfr_t y, const mpfr_t x, mpfr_rnd_t r) { mpfr_sin(y,x,r); return mpfr_cos(y,x,r); }
#endif

/* Wrappers for vector functions.  */
#ifdef __vpcs
static float Z_sinf(float x) { return _ZGVnN4v_sinf(argf(x))[0]; }
static float Z_cosf(float x) { return _ZGVnN4v_cosf(argf(x))[0]; }
static float Z_expf_1u(float x) { return _ZGVnN4v_expf_1u(argf(x))[0]; }
static float Z_expf(float x) { return _ZGVnN4v_expf(argf(x))[0]; }
static float Z_exp2f_1u(float x) { return _ZGVnN4v_exp2f_1u(argf(x))[0]; }
static float Z_exp2f(float x) { return _ZGVnN4v_exp2f(argf(x))[0]; }
static float Z_logf(float x) { return _ZGVnN4v_logf(argf(x))[0]; }
static float Z_powf(float x, float y) { return _ZGVnN4vv_powf(argf(x),argf(y))[0]; }
static double Z_sin(double x) { return _ZGVnN2v_sin(argd(x))[0]; }
static double Z_cos(double x) { return _ZGVnN2v_cos(argd(x))[0]; }
static double Z_exp(double x) { return _ZGVnN2v_exp(argd(x))[0]; }
static double Z_log(double x) { return _ZGVnN2v_log(argd(x))[0]; }
static double Z_pow(double x, double y) { return _ZGVnN2vv_pow(argd(x),argd(y))[0]; }
#endif

/* clang-format on */
