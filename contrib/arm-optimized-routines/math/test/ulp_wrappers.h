/*
 * Function wrappers for ulp.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

/* clang-format off */

#if  __aarch64__ && __linux__
#include <arm_neon.h>
#endif

#include <stdbool.h>

/* Wrappers for sincos.  */
static float sincosf_sinf(float x) {(void)cosf(x); return sinf(x);}
static float sincosf_cosf(float x) {(void)sinf(x); return cosf(x);}
static double sincos_sin(double x) {(void)cos(x); return sin(x);}
static double sincos_cos(double x) {(void)sin(x); return cos(x);}
#if USE_MPFR
static int sincos_mpfr_sin(mpfr_t y, const mpfr_t x, mpfr_rnd_t r) { mpfr_cos(y,x,r); return mpfr_sin(y,x,r); }
static int sincos_mpfr_cos(mpfr_t y, const mpfr_t x, mpfr_rnd_t r) { mpfr_sin(y,x,r); return mpfr_cos(y,x,r); }
static int modf_mpfr_frac(mpfr_t f, const mpfr_t x, mpfr_rnd_t r) { MPFR_DECL_INIT(i, 80); return mpfr_modf(i,f,x,r); }
static int modf_mpfr_int(mpfr_t i, const mpfr_t x, mpfr_rnd_t r) { MPFR_DECL_INIT(f, 80); return mpfr_modf(i,f,x,r); }
# if MPFR_VERSION < MPFR_VERSION_NUM(4, 2, 0)
static int mpfr_tanpi (mpfr_t ret, const mpfr_t arg, mpfr_rnd_t rnd) {
  MPFR_DECL_INIT (frd, 1080);
  mpfr_const_pi (frd, GMP_RNDN);
  mpfr_mul (frd, frd, arg, GMP_RNDN);
  return mpfr_tan (ret, frd, GMP_RNDN);
}
static int mpfr_sinpi (mpfr_t ret, const mpfr_t arg, mpfr_rnd_t rnd) {
  MPFR_DECL_INIT (frd, 1080);
  mpfr_const_pi (frd, GMP_RNDN);
  mpfr_mul (frd, frd, arg, GMP_RNDN);
  return mpfr_sin (ret, frd, GMP_RNDN);
}

static int mpfr_cospi (mpfr_t ret, const mpfr_t arg, mpfr_rnd_t rnd) {
  MPFR_DECL_INIT (frd, 1080);
  mpfr_const_pi (frd, GMP_RNDN);
  mpfr_mul (frd, frd, arg, GMP_RNDN);
  return mpfr_cos (ret, frd, GMP_RNDN);
}
# endif
# if WANT_EXPERIMENTAL_MATH
static int wrap_mpfr_powi(mpfr_t ret, const mpfr_t x, const mpfr_t y, mpfr_rnd_t rnd) {
  mpfr_t y2;
  mpfr_init(y2);
  mpfr_trunc(y2, y);
  return mpfr_pow(ret, x, y2, rnd);
}
# endif
#endif

float modff_frac(float x) { float i; return modff(x, &i); }
float modff_int(float x) { float i; modff(x, &i); return i; }
double modf_frac(double x) { double i; return modf(x, &i); }
double modf_int(double x) { double i; modf(x, &i); return i; }
long double modfl_frac(long double x) { long double i; return modfl(x, &i); }
long double modfl_int(long double x) { long double i; modfl(x, &i); return i; }

/* Wrappers for vector functions.  */
#if __aarch64__ && __linux__
static float Z_expf_1u(float x) { return _ZGVnN4v_expf_1u(argf(x))[0]; }
static float Z_exp2f_1u(float x) { return _ZGVnN4v_exp2f_1u(argf(x))[0]; }
#endif

/* clang-format on */

/* No wrappers for scalar routines, but TEST_SIG will emit them.  */
#define ZSNF1_WRAP(func)
#define ZSNF2_WRAP(func)
#define ZSND1_WRAP(func)
#define ZSND2_WRAP(func)

#define ZVNF1_WRAP(func)                                                      \
  static float Z_##func##f (float x)                                          \
  {                                                                           \
    return _ZGVnN4v_##func##f (argf (x))[0];                                  \
  }
#define ZVNF2_WRAP(func)                                                      \
  static float Z_##func##f (float x, float y)                                 \
  {                                                                           \
    return _ZGVnN4vv_##func##f (argf (x), argf (y))[0];                       \
  }
#define ZVND1_WRAP(func)                                                      \
  static double Z_##func (double x) { return _ZGVnN2v_##func (argd (x))[0]; }
#define ZVND2_WRAP(func)                                                      \
  static double Z_##func (double x, double y)                                 \
  {                                                                           \
    return _ZGVnN2vv_##func (argd (x), argd (y))[0];                          \
  }

#if WANT_TRIGPI_TESTS
float
arm_math_sincospif_sin (float x)
{
  float s, c;
  arm_math_sincospif (x, &s, &c);
  return s;
}
float
arm_math_sincospif_cos (float x)
{
  float s, c;
  arm_math_sincospif (x, &s, &c);
  return c;
}
double
arm_math_sincospi_sin (double x)
{
  double s, c;
  arm_math_sincospi (x, &s, &c);
  return s;
}
double
arm_math_sincospi_cos (double x)
{
  double s, c;
  arm_math_sincospi (x, &s, &c);
  return c;
}
#endif

#if  __aarch64__ && __linux__

# if WANT_TRIGPI_TESTS
ZVNF1_WRAP (cospi)
ZVND1_WRAP (cospi)
ZVNF1_WRAP (sinpi)
ZVND1_WRAP (sinpi)
ZVNF1_WRAP (tanpi)
ZVND1_WRAP (tanpi)

double
v_sincospi_sin (double x)
{
  double s[2], c[2];
  _ZGVnN2vl8l8_sincospi (vdupq_n_f64 (x), s, c);
  return s[0];
}
double
v_sincospi_cos (double x)
{
  double s[2], c[2];
  _ZGVnN2vl8l8_sincospi (vdupq_n_f64 (x), s, c);
  return c[0];
}
float
v_sincospif_sin (float x)
{
  float s[4], c[4];
  _ZGVnN4vl4l4_sincospif (vdupq_n_f32 (x), s, c);
  return s[0];
}
float
v_sincospif_cos (float x)
{
  float s[4], c[4];
  _ZGVnN4vl4l4_sincospif (vdupq_n_f32 (x), s, c);
  return c[0];
}
# endif // WANT_TRIGPI_TESTS

float
v_sincosf_sin (float x)
{
  float s[4], c[4];
  _ZGVnN4vl4l4_sincosf (vdupq_n_f32 (x), s, c);
  return s[0];
}
float
v_sincosf_cos (float x)
{
  float s[4], c[4];
  _ZGVnN4vl4l4_sincosf (vdupq_n_f32 (x), s, c);
  return c[0];
}
float
v_cexpif_sin (float x)
{
  return _ZGVnN4v_cexpif (vdupq_n_f32 (x)).val[0][0];
}
float
v_cexpif_cos (float x)
{
  return _ZGVnN4v_cexpif (vdupq_n_f32 (x)).val[1][0];
}
float
v_modff_frac (float x)
{
  float y[4];
  return _ZGVnN4vl4_modff (vdupq_n_f32 (x), y)[0];
}
float
v_modff_int (float x)
{
  float y[4];
  _ZGVnN4vl4_modff (vdupq_n_f32 (x), y);
  return y[0];
}
double
v_sincos_sin (double x)
{
  double s[2], c[2];
  _ZGVnN2vl8l8_sincos (vdupq_n_f64 (x), s, c);
  return s[0];
}
double
v_sincos_cos (double x)
{
  double s[2], c[2];
  _ZGVnN2vl8l8_sincos (vdupq_n_f64 (x), s, c);
  return c[0];
}
double
v_cexpi_sin (double x)
{
  return _ZGVnN2v_cexpi (vdupq_n_f64 (x)).val[0][0];
}
double
v_cexpi_cos (double x)
{
  return _ZGVnN2v_cexpi (vdupq_n_f64 (x)).val[1][0];
}
double
v_modf_frac (double x)
{
  double y[2];
  return _ZGVnN2vl8_modf (vdupq_n_f64 (x), y)[0];
}
double
v_modf_int (double x)
{
  double y[2];
  _ZGVnN2vl8_modf (vdupq_n_f64 (x), y);
  return y[0];
}
#endif //  __aarch64__ && __linux__

#if WANT_SVE_TESTS
# define ZSVNF1_WRAP(func)                                                   \
    static float Z_sv_##func##f (svbool_t pg, float x)                        \
    {                                                                         \
      return svretf (_ZGVsMxv_##func##f (svargf (x), pg), pg);                \
    }
# define ZSVNF2_WRAP(func)                                                   \
    static float Z_sv_##func##f (svbool_t pg, float x, float y)               \
    {                                                                         \
      return svretf (_ZGVsMxvv_##func##f (svargf (x), svargf (y), pg), pg);   \
    }
# define ZSVND1_WRAP(func)                                                   \
    static double Z_sv_##func (svbool_t pg, double x)                         \
    {                                                                         \
      return svretd (_ZGVsMxv_##func (svargd (x), pg), pg);                   \
    }
# define ZSVND2_WRAP(func)                                                   \
    static double Z_sv_##func (svbool_t pg, double x, double y)               \
    {                                                                         \
      return svretd (_ZGVsMxvv_##func (svargd (x), svargd (y), pg), pg);      \
    }

# if WANT_TRIGPI_TESTS
ZSVNF1_WRAP (cospi)
ZSVND1_WRAP (cospi)
ZSVNF1_WRAP (sinpi)
ZSVND1_WRAP (sinpi)
ZSVNF1_WRAP (tanpi)
ZSVND1_WRAP (tanpi)
double
sv_sincospi_sin (svbool_t pg, double x)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincospi (svdup_f64 (x), s, c, pg);
  return svretd (svld1 (pg, s), pg);
}
double
sv_sincospi_cos (svbool_t pg, double x)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincospi (svdup_f64 (x), s, c, pg);
  return svretd (svld1 (pg, c), pg);
}
float
sv_sincospif_sin (svbool_t pg, float x)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincospif (svdup_f32 (x), s, c, pg);
  return svretf (svld1 (pg, s), pg);
}
float
sv_sincospif_cos (svbool_t pg, float x)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincospif (svdup_f32 (x), s, c, pg);
  return svretf (svld1 (pg, c), pg);
}
# endif // WANT_TRIGPI_TESTS

float
sv_sincosf_sin (svbool_t pg, float x)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincosf (svdup_f32 (x), s, c, pg);
  return svretf (svld1 (pg, s), pg);
}
float
sv_sincosf_cos (svbool_t pg, float x)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincosf (svdup_f32 (x), s, c, pg);
  return svretf (svld1 (pg, c), pg);
}
float
sv_cexpif_sin (svbool_t pg, float x)
{
  return svretf (svget2 (_ZGVsMxv_cexpif (svdup_f32 (x), pg), 0), pg);
}
float
sv_cexpif_cos (svbool_t pg, float x)
{
  return svretf (svget2 (_ZGVsMxv_cexpif (svdup_f32 (x), pg), 1), pg);
}
float
sv_modff_frac (svbool_t pg, float x)
{
  float i[svcntw ()];
  return svretf (_ZGVsMxvl4_modff (svdup_f32 (x), i, pg), pg);
}
float
sv_modff_int (svbool_t pg, float x)
{
  float i[svcntw ()];
  _ZGVsMxvl4_modff (svdup_f32 (x), i, pg);
  return svretf (svld1 (pg, i), pg);
}
double
sv_sincos_sin (svbool_t pg, double x)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincos (svdup_f64 (x), s, c, pg);
  return svretd (svld1 (pg, s), pg);
}
double
sv_sincos_cos (svbool_t pg, double x)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincos (svdup_f64 (x), s, c, pg);
  return svretd (svld1 (pg, c), pg);
}
double
sv_cexpi_sin (svbool_t pg, double x)
{
  return svretd (svget2 (_ZGVsMxv_cexpi (svdup_f64 (x), pg), 0), pg);
}
double
sv_cexpi_cos (svbool_t pg, double x)
{
  return svretd (svget2 (_ZGVsMxv_cexpi (svdup_f64 (x), pg), 1), pg);
}
double
sv_modf_frac (svbool_t pg, double x)
{
  double i[svcntd ()];
  return svretd (_ZGVsMxvl8_modf (svdup_f64 (x), i, pg), pg);
}
double
sv_modf_int (svbool_t pg, double x)
{
  double i[svcntd ()];
  _ZGVsMxvl8_modf (svdup_f64 (x), i, pg);
  return svretd (svld1 (pg, i), pg);
}

# if WANT_EXPERIMENTAL_MATH

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
#  define DECL_POW_INT_REF(NAME, DBL_T, FLT_T, INT_T)                       \
      static DBL_T __attribute__ ((unused)) NAME (DBL_T in_val, DBL_T y)      \
      {                                                                       \
	INT_T n = (INT_T) round (y);                                          \
	FLT_T acc = 1.0;                                                      \
	bool want_recip = n < 0;                                              \
	n = n < 0 ? -n : n;                                                   \
                                                                              \
	for (FLT_T c = in_val; n; c *= c, n >>= 1)                            \
	  {                                                                   \
	    if (n & 0x1)                                                      \
	      {                                                               \
		acc *= c;                                                     \
	      }                                                               \
	  }                                                                   \
	if (want_recip)                                                       \
	  {                                                                   \
	    acc = 1.0 / acc;                                                  \
	  }                                                                   \
	return acc;                                                           \
      }

DECL_POW_INT_REF (ref_powif, double, float, int)
DECL_POW_INT_REF (ref_powi, long double, double, int)
static float
Z_sv_powi (svbool_t pg, float x, float y)
{
  return svretf (_ZGVsMxvv_powi (svargf (x), svdup_s32 ((int) round (y)), pg),
		 pg);
}
static double
Z_sv_powk (svbool_t pg, double x, double y)
{
  return svretd (_ZGVsMxvv_powk (svargd (x), svdup_s64 ((long) round (y)), pg),
		 pg);
}

# endif // WANT_EXPERIMENTAL_MATH
#endif	// WANT_SVE_TESTS

#include "test/ulp_wrappers_gen.h"
