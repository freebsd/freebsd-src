// clang-format off
/*
 * Function wrappers for ulp.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <stdbool.h>

#if USE_MPFR
static int sincos_mpfr_sin(mpfr_t y, const mpfr_t x, mpfr_rnd_t r) {
  mpfr_cos(y, x, r);
  return mpfr_sin(y, x, r);
}
static int sincos_mpfr_cos(mpfr_t y, const mpfr_t x, mpfr_rnd_t r) {
  mpfr_sin(y, x, r);
  return mpfr_cos(y, x, r);
}
static int wrap_mpfr_powi(mpfr_t ret, const mpfr_t x, const mpfr_t y, mpfr_rnd_t rnd) {
  mpfr_t y2;
  mpfr_init(y2);
  mpfr_trunc(y2, y);
  return mpfr_pow(ret, x, y2, rnd);
}
#endif

/* Our implementations of powi/powk are too imprecise to verify
   against any established pow implementation. Instead we have the
   following simple implementation, against which it is enough to
   maintain bitwise reproducibility. Note the test framework expects
   the reference impl to be of higher precision than the function
   under test. For instance this means that the reference for
   double-precision powi will be passed a long double, so to check
   bitwise reproducibility we have to cast it back down to
   double. This is fine since a round-trip to higher precision and
   back down is correctly rounded.  */
#define DECL_POW_INT_REF(NAME, DBL_T, FLT_T, INT_T)                            \
  static DBL_T NAME (DBL_T in_val, DBL_T y)                                    \
  {                                                                            \
    INT_T n = (INT_T) round (y);                                               \
    FLT_T acc = 1.0;                                                           \
    bool want_recip = n < 0;                                                   \
    n = n < 0 ? -n : n;                                                        \
                                                                               \
    for (FLT_T c = in_val; n; c *= c, n >>= 1)                                 \
      {                                                                        \
        if (n & 0x1)                                                           \
          {                                                                    \
            acc *= c;                                                          \
          }                                                                    \
      }                                                                        \
    if (want_recip)                                                            \
      {                                                                        \
        acc = 1.0 / acc;                                                       \
      }                                                                        \
    return acc;                                                                \
  }

DECL_POW_INT_REF(ref_powif, double, float, int)
DECL_POW_INT_REF(ref_powi, long double, double, int)

#define VF1_WRAP(func) static float v_##func##f(float x) { return __v_##func##f(argf(x))[0]; }
#define VF2_WRAP(func) static float v_##func##f(float x, float y) { return __v_##func##f(argf(x), argf(y))[0]; }
#define VD1_WRAP(func) static double v_##func(double x) { return __v_##func(argd(x))[0]; }
#define VD2_WRAP(func) static double v_##func(double x, double y) { return __v_##func(argd(x), argd(y))[0]; }

#define VNF1_WRAP(func) static float vn_##func##f(float x) { return __vn_##func##f(argf(x))[0]; }
#define VNF2_WRAP(func) static float vn_##func##f(float x, float y) { return __vn_##func##f(argf(x), argf(y))[0]; }
#define VND1_WRAP(func) static double vn_##func(double x) { return __vn_##func(argd(x))[0]; }
#define VND2_WRAP(func) static double vn_##func(double x, double y) { return __vn_##func(argd(x), argd(y))[0]; }

#define ZVF1_WRAP(func) static float Z_##func##f(float x) { return _ZGVnN4v_##func##f(argf(x))[0]; }
#define ZVF2_WRAP(func) static float Z_##func##f(float x, float y) { return _ZGVnN4vv_##func##f(argf(x), argf(y))[0]; }
#define ZVD1_WRAP(func) static double Z_##func(double x) { return _ZGVnN2v_##func(argd(x))[0]; }
#define ZVD2_WRAP(func) static double Z_##func(double x, double y) { return _ZGVnN2vv_##func(argd(x), argd(y))[0]; }

#ifdef __vpcs

#define ZVNF1_WRAP(func) VF1_WRAP(func) VNF1_WRAP(func) ZVF1_WRAP(func)
#define ZVNF2_WRAP(func) VF2_WRAP(func) VNF2_WRAP(func) ZVF2_WRAP(func)
#define ZVND1_WRAP(func) VD1_WRAP(func) VND1_WRAP(func) ZVD1_WRAP(func)
#define ZVND2_WRAP(func) VD2_WRAP(func) VND2_WRAP(func) ZVD2_WRAP(func)

#elif __aarch64__

#define ZVNF1_WRAP(func) VF1_WRAP(func) VNF1_WRAP(func)
#define ZVNF2_WRAP(func) VF2_WRAP(func) VNF2_WRAP(func)
#define ZVND1_WRAP(func) VD1_WRAP(func) VND1_WRAP(func)
#define ZVND2_WRAP(func) VD2_WRAP(func) VND2_WRAP(func)

#elif WANT_VMATH

#define ZVNF1_WRAP(func) VF1_WRAP(func)
#define ZVNF2_WRAP(func) VF2_WRAP(func)
#define ZVND1_WRAP(func) VD1_WRAP(func)
#define ZVND2_WRAP(func) VD2_WRAP(func)

#else

#define ZVNF1_WRAP(func)
#define ZVNF2_WRAP(func)
#define ZVND1_WRAP(func)
#define ZVND2_WRAP(func)

#endif

#define SVF1_WRAP(func) static float sv_##func##f(float x) { return svretf(__sv_##func##f_x(svargf(x), svptrue_b32())); }
#define SVF2_WRAP(func) static float sv_##func##f(float x, float y) { return svretf(__sv_##func##f_x(svargf(x), svargf(y), svptrue_b32())); }
#define SVD1_WRAP(func) static double sv_##func(double x) { return svretd(__sv_##func##_x(svargd(x), svptrue_b64())); }
#define SVD2_WRAP(func) static double sv_##func(double x, double y) { return svretd(__sv_##func##_x(svargd(x), svargd(y), svptrue_b64())); }

#define ZSVF1_WRAP(func) static float Z_sv_##func##f(float x) { return svretf(_ZGVsMxv_##func##f(svargf(x), svptrue_b32())); }
#define ZSVF2_WRAP(func) static float Z_sv_##func##f(float x, float y) { return svretf(_ZGVsMxvv_##func##f(svargf(x), svargf(y), svptrue_b32())); }
#define ZSVD1_WRAP(func) static double Z_sv_##func(double x) { return svretd(_ZGVsMxv_##func(svargd(x), svptrue_b64())); }
#define ZSVD2_WRAP(func) static double Z_sv_##func(double x, double y) { return svretd(_ZGVsMxvv_##func(svargd(x), svargd(y), svptrue_b64())); }

#if WANT_SVE_MATH

#define ZSVNF1_WRAP(func) SVF1_WRAP(func) ZSVF1_WRAP(func)
#define ZSVNF2_WRAP(func) SVF2_WRAP(func) ZSVF2_WRAP(func)
#define ZSVND1_WRAP(func) SVD1_WRAP(func) ZSVD1_WRAP(func)
#define ZSVND2_WRAP(func) SVD2_WRAP(func) ZSVD2_WRAP(func)

#else

#define ZSVNF1_WRAP(func)
#define ZSVNF2_WRAP(func)
#define ZSVND1_WRAP(func)
#define ZSVND2_WRAP(func)

#endif

/* No wrappers for scalar routines, but PL_SIG will emit them.  */
#define ZSNF1_WRAP(func)
#define ZSNF2_WRAP(func)
#define ZSND1_WRAP(func)
#define ZSND2_WRAP(func)

#include "ulp_wrappers_gen.h"

#if WANT_SVE_MATH
static float Z_sv_powi(float x, float y) { return svretf(_ZGVsMxvv_powi(svargf(x), svdup_n_s32((int)round(y)), svptrue_b32())); }
static float sv_powif(float x, float y) { return svretf(__sv_powif_x(svargf(x), svdup_n_s32((int)round(y)), svptrue_b32())); }
static double Z_sv_powk(double x, double y) { return svretd(_ZGVsMxvv_powk(svargd(x), svdup_n_s64((long)round(y)), svptrue_b64())); }
static double sv_powi(double x, double y) { return svretd(__sv_powi_x(svargd(x), svdup_n_s64((long)round(y)), svptrue_b64())); }
#endif
// clang-format on
