/*
 * Public API.
 *
 * Copyright (c) 2015-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef _MATHLIB_H
#define _MATHLIB_H

float expf (float);
float exp2f (float);
float logf (float);
float log2f (float);
float powf (float, float);
float sinf (float);
float cosf (float);
void sincosf (float, float*, float*);

double exp (double);
double exp10 (double);
double exp2 (double);
double log (double);
double log2 (double);
double pow (double, double);

#if __aarch64__
# if __GNUC__ >= 5
typedef __Float32x4_t __f32x4_t;
typedef __Float64x2_t __f64x2_t;
# elif __clang_major__*100+__clang_minor__ >= 305
typedef __attribute__((__neon_vector_type__(4))) float __f32x4_t;
typedef __attribute__((__neon_vector_type__(2))) double __f64x2_t;
# else
#  error Unsupported compiler
# endif

# if __GNUC__ >= 9 || __clang_major__ >= 8
#  undef __vpcs
#  define __vpcs __attribute__((__aarch64_vector_pcs__))

/* Vector functions following the vector PCS using ABI names.  */
__vpcs __f32x4_t _ZGVnN4v_sinf (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4v_cosf (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4v_expf_1u (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4v_expf (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4v_exp2f_1u (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4v_exp2f (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4v_logf (__f32x4_t);
__vpcs __f32x4_t _ZGVnN4vv_powf (__f32x4_t, __f32x4_t);
__vpcs __f64x2_t _ZGVnN2v_sin (__f64x2_t);
__vpcs __f64x2_t _ZGVnN2v_cos (__f64x2_t);
__vpcs __f64x2_t _ZGVnN2v_exp (__f64x2_t);
__vpcs __f64x2_t _ZGVnN2v_log (__f64x2_t);
__vpcs __f64x2_t _ZGVnN2vv_pow (__f64x2_t, __f64x2_t);
# endif
#endif

#endif
