/*
 * Macros for emitting various ulp/bench entries based on function signature
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception.
 */

#define TEST_DECL_SF1(fun) float fun##f (float);
#define TEST_DECL_SF2(fun) float fun##f (float, float);
#define TEST_DECL_SD1(fun) double fun (double);
#define TEST_DECL_SD2(fun) double fun (double, double);

#define TEST_DECL_VF1(fun)                                                    \
  float32x4_t VPCS_ATTR V_NAME_F1 (fun##f) (float32x4_t);
#define TEST_DECL_VF2(fun)                                                    \
  float32x4_t VPCS_ATTR V_NAME_F2 (fun##f) (float32x4_t, float32x4_t);
#define TEST_DECL_VD1(fun) VPCS_ATTR float64x2_t V_NAME_D1 (fun) (float64x2_t);
#define TEST_DECL_VD2(fun)                                                    \
  VPCS_ATTR float64x2_t V_NAME_D2 (fun) (float64x2_t, float64x2_t);

#define TEST_DECL_SVF1(fun)                                                   \
  svfloat32_t SV_NAME_F1 (fun) (svfloat32_t, svbool_t);
#define TEST_DECL_SVF2(fun)                                                   \
  svfloat32_t SV_NAME_F2 (fun) (svfloat32_t, svfloat32_t, svbool_t);
#define TEST_DECL_SVD1(fun)                                                   \
  svfloat64_t SV_NAME_D1 (fun) (svfloat64_t, svbool_t);
#define TEST_DECL_SVD2(fun)                                                   \
  svfloat64_t SV_NAME_D2 (fun) (svfloat64_t, svfloat64_t, svbool_t);

/* For building the routines, emit function prototype from TEST_SIG. This
   ensures that the correct signature has been chosen (wrong one will be a
   compile error). TEST_SIG is defined differently by various components of the
   build system to emit entries in the wrappers and entries for mathbench and
   ulp.  */
#ifndef _TEST_SIG
# if defined(EMIT_ULP_FUNCS)
#  define _TEST_SIG(v, t, a, f, ...) TEST_SIG _Z##v##t##a (f)
# elif defined(EMIT_ULP_WRAPPERS)
#  define _TEST_SIG(v, t, a, f, ...) TEST_SIG Z##v##N##t##a##_WRAP (f)
# elif defined(EMIT_MATHBENCH_FUNCS)
#  define _TEST_SIG(v, t, a, f, ...) TEST_SIG _Z##v##t##a (f, ##__VA_ARGS__)
# else
#  define _TEST_SIG(v, t, a, f, ...) TEST_DECL_##v##t##a (f)
# endif
#endif

#define TEST_SIG(...) _TEST_SIG (__VA_ARGS__)
