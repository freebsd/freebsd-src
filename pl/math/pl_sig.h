/*
 * PL macros for emitting various ulp/bench entries based on function signature
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception.
 */
#define PL_DECL_SF1(fun) float fun##f (float);
#define PL_DECL_SF2(fun) float fun##f (float, float);
#define PL_DECL_SD1(fun) double fun (double);
#define PL_DECL_SD2(fun) double fun (double, double);

#if V_SUPPORTED
#define PL_DECL_VF1(fun) VPCS_ATTR v_f32_t V_NAME (fun##f) (v_f32_t);
#define PL_DECL_VF2(fun) VPCS_ATTR v_f32_t V_NAME (fun##f) (v_f32_t, v_f32_t);
#define PL_DECL_VD1(fun) VPCS_ATTR v_f64_t V_NAME (fun) (v_f64_t);
#define PL_DECL_VD2(fun) VPCS_ATTR v_f64_t V_NAME (fun) (v_f64_t, v_f64_t);
#else
#define PL_DECL_VF1(fun)
#define PL_DECL_VF2(fun)
#define PL_DECL_VD1(fun)
#define PL_DECL_VD2(fun)
#endif

#if SV_SUPPORTED
#define PL_DECL_SVF1(fun) sv_f32_t __sv_##fun##f_x (sv_f32_t, svbool_t);
#define PL_DECL_SVF2(fun)                                                      \
  sv_f32_t __sv_##fun##f_x (sv_f32_t, sv_f32_t, svbool_t);
#define PL_DECL_SVD1(fun) sv_f64_t __sv_##fun##_x (sv_f64_t, svbool_t);
#define PL_DECL_SVD2(fun)                                                      \
  sv_f64_t __sv_##fun##_x (sv_f64_t, sv_f64_t, svbool_t);
#else
#define PL_DECL_SVF1(fun)
#define PL_DECL_SVF2(fun)
#define PL_DECL_SVD1(fun)
#define PL_DECL_SVD2(fun)
#endif

/* For building the routines, emit function prototype from PL_SIG. This
   ensures that the correct signature has been chosen (wrong one will be a
   compile error). PL_SIG is defined differently by various components of the
   build system to emit entries in the wrappers and entries for mathbench and
   ulp.  */
#define PL_SIG(v, t, a, f, ...) PL_DECL_##v##t##a (f)
