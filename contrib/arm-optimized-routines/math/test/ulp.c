/*
 * ULP error checking tool for math functions.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if WANT_SVE_TESTS
#  if __aarch64__ && __linux__
#    ifdef __clang__
#      pragma clang attribute push(__attribute__((target("sve"))),            \
				   apply_to = any(function))
#    else
#      pragma GCC target("+sve")
#    endif
#  else
#    error "SVE not supported - please disable WANT_SVE_TESTS"
#  endif
#endif

#define _GNU_SOURCE
#include <ctype.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mathlib.h"

#include "trigpi_references.h"

/* Don't depend on mpfr by default.  */
#ifndef USE_MPFR
# define USE_MPFR 0
#endif
#if USE_MPFR
# include <mpfr.h>
#endif

static uint64_t seed = 0x0123456789abcdef;
static uint64_t
rand64 (void)
{
  seed = 6364136223846793005ull * seed + 1;
  return seed ^ (seed >> 32);
}

/* Uniform random in [0,n].  */
static uint64_t
randn (uint64_t n)
{
  uint64_t r, m;

  if (n == 0)
    return 0;
  n++;
  if (n == 0)
    return rand64 ();
  for (;;)
    {
      r = rand64 ();
      m = r % n;
      if (r - m <= -n)
	return m;
    }
}

struct gen
{
  uint64_t start;
  uint64_t len;
  uint64_t start2;
  uint64_t len2;
  uint64_t off;
  uint64_t step;
  uint64_t cnt;
};

struct args_f1
{
  float x;
};

struct args_f2
{
  float x;
  float x2;
};

struct args_d1
{
  double x;
};

struct args_d2
{
  double x;
  double x2;
};

/* result = y + tail*2^ulpexp.  */
struct ret_f
{
  float y;
  double tail;
  int ulpexp;
  int ex;
  int ex_may;
};

struct ret_d
{
  double y;
  double tail;
  int ulpexp;
  int ex;
  int ex_may;
};

static inline uint64_t
next1 (struct gen *g)
{
  /* For single argument use randomized incremental steps,
     that produce dense sampling without collisions and allow
     testing all inputs in a range.  */
  uint64_t r = g->start + g->off;
  g->off += g->step + randn (g->step / 2);
  if (g->off > g->len)
    g->off -= g->len; /* hack.  */
  return r;
}

static inline uint64_t
next2 (uint64_t *x2, struct gen *g)
{
  /* For two arguments use uniform random sampling.  */
  uint64_t r = g->start + randn (g->len);
  *x2 = g->start2 + randn (g->len2);
  return r;
}

static struct args_f1
next_f1 (void *g)
{
  return (struct args_f1){asfloat (next1 (g))};
}

static struct args_f2
next_f2 (void *g)
{
  uint64_t x2;
  uint64_t x = next2 (&x2, g);
  return (struct args_f2){asfloat (x), asfloat (x2)};
}

static struct args_d1
next_d1 (void *g)
{
  return (struct args_d1){asdouble (next1 (g))};
}

static struct args_d2
next_d2 (void *g)
{
  uint64_t x2;
  uint64_t x = next2 (&x2, g);
  return (struct args_d2){asdouble (x), asdouble (x2)};
}

/* A bit of a hack: call vector functions twice with the same
   input in lane 0 but a different value in other lanes: once
   with an in-range value and then with a special case value.  */
static int secondcall;

/* Wrappers for vector functions.  */
#if __aarch64__ && __linux__
/* First element of fv and dv may be changed by -c argument.  */
static float fv[2] = {1.0f, -INFINITY};
static double dv[2] = {1.0, -INFINITY};
static inline float32x4_t
argf (float x)
{
  return (float32x4_t){ x, x, x, fv[secondcall] };
}
static inline float64x2_t
argd (double x)
{
  return (float64x2_t){ x, dv[secondcall] };
}
#if WANT_SVE_TESTS
#include <arm_sve.h>

static inline svfloat32_t
svargf (float x)
{
  int n = svcntw ();
  float base[n];
  for (int i = 0; i < n; i++)
    base[i] = (float) x;
  base[n - 1] = (float) fv[secondcall];
  return svld1 (svptrue_b32 (), base);
}
static inline svfloat64_t
svargd (double x)
{
  int n = svcntd ();
  double base[n];
  for (int i = 0; i < n; i++)
    base[i] = x;
  base[n - 1] = dv[secondcall];
  return svld1 (svptrue_b64 (), base);
}
static inline float
svretf (svfloat32_t vec, svbool_t pg)
{
  return svlastb_f32 (svpfirst (pg, svpfalse ()), vec);
}
static inline double
svretd (svfloat64_t vec, svbool_t pg)
{
  return svlastb_f64 (svpfirst (pg, svpfalse ()), vec);
}

static inline svbool_t
parse_pg (uint64_t p, int is_single)
{
  if (is_single)
    {
      uint32_t tmp[svcntw ()];
      for (unsigned i = 0; i < svcntw (); i++)
	tmp[i] = (p >> i) & 1;
      return svcmpne (svptrue_b32 (), svld1 (svptrue_b32 (), tmp), 0);
    }
  else
    {
      uint64_t tmp[svcntd ()];
      for (unsigned i = 0; i < svcntd (); i++)
	tmp[i] = (p >> i) & 1;
      return svcmpne (svptrue_b64 (), svld1 (svptrue_b64 (), tmp), 0);
    }
}
# endif
#endif

struct conf
{
  int r;
  int rc;
  int quiet;
  int mpfr;
  int fenv;
  unsigned long long n;
  double softlim;
  double errlim;
  int ignore_zero_sign;
#if WANT_SVE_TESTS
  svbool_t *pg;
#endif
};

#include "test/ulp_wrappers.h"

struct fun
{
  const char *name;
  int arity;
  int singleprec;
  int twice;
  int is_predicated;
  union
  {
    float (*f1) (float);
    float (*f2) (float, float);
    double (*d1) (double);
    double (*d2) (double, double);
#if WANT_SVE_TESTS
    float (*f1_pred) (svbool_t, float);
    float (*f2_pred) (svbool_t, float, float);
    double (*d1_pred) (svbool_t, double);
    double (*d2_pred) (svbool_t, double, double);
#endif
  } fun;
  union
  {
    double (*f1) (double);
    double (*f2) (double, double);
    long double (*d1) (long double);
    long double (*d2) (long double, long double);
  } fun_long;
#if USE_MPFR
  union
  {
    int (*f1) (mpfr_t, const mpfr_t, mpfr_rnd_t);
    int (*f2) (mpfr_t, const mpfr_t, const mpfr_t, mpfr_rnd_t);
    int (*d1) (mpfr_t, const mpfr_t, mpfr_rnd_t);
    int (*d2) (mpfr_t, const mpfr_t, const mpfr_t, mpfr_rnd_t);
  } fun_mpfr;
#endif
};

// clang-format off
static const struct fun fun[] = {
#if USE_MPFR
#  define F(x, x_wrap, x_long, x_mpfr, a, s, t, twice)                        \
    { #x, a, s, twice, 0, { .t = x_wrap }, { .t = x_long }, { .t = x_mpfr } },
#  define SVF(x, x_wrap, x_long, x_mpfr, a, s, t, twice)                      \
    { #x, a, s, twice, 1, { .t##_pred = x_wrap }, { .t = x_long }, { .t = x_mpfr } },
#else
#  define F(x, x_wrap, x_long, x_mpfr, a, s, t, twice)                        \
    { #x, a, s, twice, 0, { .t = x_wrap }, { .t = x_long } },
#  define SVF(x, x_wrap, x_long, x_mpfr, a, s, t, twice)                      \
    { #x, a, s, twice, 1, { .t##_pred = x_wrap }, { .t = x_long } },
#endif
#define F1(x) F (x##f, x##f, x, mpfr_##x, 1, 1, f1, 0)
#define F2(x) F (x##f, x##f, x, mpfr_##x, 2, 1, f2, 0)
#define D1(x) F (x, x, x##l, mpfr_##x, 1, 0, d1, 0)
#define D2(x) F (x, x, x##l, mpfr_##x, 2, 0, d2, 0)
/* Neon routines.  */
#define ZVNF1(x) F (_ZGVnN4v_##x##f, Z_##x##f, x, mpfr_##x, 1, 1, f1, 0)
#define ZVNF2(x) F (_ZGVnN4vv_##x##f, Z_##x##f, x, mpfr_##x, 2, 1, f2, 0)
#define ZVND1(x) F (_ZGVnN2v_##x, Z_##x, x##l, mpfr_##x, 1, 0, d1, 0)
#define ZVND2(x) F (_ZGVnN2vv_##x, Z_##x, x##l, mpfr_##x, 2, 0, d2, 0)
/* SVE routines.  */
#define ZSVF1(x) SVF (_ZGVsMxv_##x##f, Z_sv_##x##f, x, mpfr_##x, 1, 1, f1, 0)
#define ZSVF2(x) SVF (_ZGVsMxvv_##x##f, Z_sv_##x##f, x, mpfr_##x, 2, 1, f2, 0)
#define ZSVD1(x) SVF (_ZGVsMxv_##x, Z_sv_##x, x##l, mpfr_##x, 1, 0, d1, 0)
#define ZSVD2(x) SVF (_ZGVsMxvv_##x, Z_sv_##x, x##l, mpfr_##x, 2, 0, d2, 0)

#include "test/ulp_funcs.h"

#undef F
#undef F1
#undef F2
#undef D1
#undef D2
#undef ZSVF1
#undef ZSVF2
#undef ZSVD1
#undef ZSVD2
  { 0 }
};
// clang-format on

/* Boilerplate for generic calls.  */

static inline int
ulpscale_f (float x)
{
  int e = asuint (x) >> 23 & 0xff;
  if (!e)
    e++;
  return e - 0x7f - 23;
}
static inline int
ulpscale_d (double x)
{
  int e = asuint64 (x) >> 52 & 0x7ff;
  if (!e)
    e++;
  return e - 0x3ff - 52;
}
static inline float
call_f1 (const struct fun *f, struct args_f1 a, const struct conf *conf)
{
#if WANT_SVE_TESTS
  if (f->is_predicated)
    return f->fun.f1_pred (*conf->pg, a.x);
#endif
  return f->fun.f1 (a.x);
}
static inline float
call_f2 (const struct fun *f, struct args_f2 a, const struct conf *conf)
{
#if WANT_SVE_TESTS
  if (f->is_predicated)
    return f->fun.f2_pred (*conf->pg, a.x, a.x2);
#endif
  return f->fun.f2 (a.x, a.x2);
}

static inline double
call_d1 (const struct fun *f, struct args_d1 a, const struct conf *conf)
{
#if WANT_SVE_TESTS
  if (f->is_predicated)
    return f->fun.d1_pred (*conf->pg, a.x);
#endif
  return f->fun.d1 (a.x);
}
static inline double
call_d2 (const struct fun *f, struct args_d2 a, const struct conf *conf)
{
#if WANT_SVE_TESTS
  if (f->is_predicated)
    return f->fun.d2_pred (*conf->pg, a.x, a.x2);
#endif
  return f->fun.d2 (a.x, a.x2);
}
static inline double
call_long_f1 (const struct fun *f, struct args_f1 a)
{
  return f->fun_long.f1 (a.x);
}
static inline double
call_long_f2 (const struct fun *f, struct args_f2 a)
{
  return f->fun_long.f2 (a.x, a.x2);
}
static inline long double
call_long_d1 (const struct fun *f, struct args_d1 a)
{
  return f->fun_long.d1 (a.x);
}
static inline long double
call_long_d2 (const struct fun *f, struct args_d2 a)
{
  return f->fun_long.d2 (a.x, a.x2);
}
static inline void
printcall_f1 (const struct fun *f, struct args_f1 a)
{
  printf ("%s(%a)", f->name, a.x);
}
static inline void
printcall_f2 (const struct fun *f, struct args_f2 a)
{
  printf ("%s(%a, %a)", f->name, a.x, a.x2);
}
static inline void
printcall_d1 (const struct fun *f, struct args_d1 a)
{
  printf ("%s(%a)", f->name, a.x);
}
static inline void
printcall_d2 (const struct fun *f, struct args_d2 a)
{
  printf ("%s(%a, %a)", f->name, a.x, a.x2);
}
static inline void
printgen_f1 (const struct fun *f, struct gen *gen)
{
  printf ("%s in [%a;%a]", f->name, asfloat (gen->start),
	  asfloat (gen->start + gen->len));
}
static inline void
printgen_f2 (const struct fun *f, struct gen *gen)
{
  printf ("%s in [%a;%a] x [%a;%a]", f->name, asfloat (gen->start),
	  asfloat (gen->start + gen->len), asfloat (gen->start2),
	  asfloat (gen->start2 + gen->len2));
}
static inline void
printgen_d1 (const struct fun *f, struct gen *gen)
{
  printf ("%s in [%a;%a]", f->name, asdouble (gen->start),
	  asdouble (gen->start + gen->len));
}
static inline void
printgen_d2 (const struct fun *f, struct gen *gen)
{
  printf ("%s in [%a;%a] x [%a;%a]", f->name, asdouble (gen->start),
	  asdouble (gen->start + gen->len), asdouble (gen->start2),
	  asdouble (gen->start2 + gen->len2));
}

#define reduce_f1(a, f, op) (f (a.x))
#define reduce_f2(a, f, op) (f (a.x) op f (a.x2))
#define reduce_d1(a, f, op) (f (a.x))
#define reduce_d2(a, f, op) (f (a.x) op f (a.x2))

#ifndef IEEE_754_2008_SNAN
# define IEEE_754_2008_SNAN 1
#endif
static inline int
issignaling_f (float x)
{
  uint32_t ix = asuint (x);
  if (!IEEE_754_2008_SNAN)
    return (ix & 0x7fc00000) == 0x7fc00000;
  return 2 * (ix ^ 0x00400000) > 2u * 0x7fc00000;
}
static inline int
issignaling_d (double x)
{
  uint64_t ix = asuint64 (x);
  if (!IEEE_754_2008_SNAN)
    return (ix & 0x7ff8000000000000) == 0x7ff8000000000000;
  return 2 * (ix ^ 0x0008000000000000) > 2 * 0x7ff8000000000000ULL;
}

#if USE_MPFR
static mpfr_rnd_t
rmap (int r)
{
  switch (r)
    {
    case FE_TONEAREST:
      return MPFR_RNDN;
    case FE_TOWARDZERO:
      return MPFR_RNDZ;
    case FE_UPWARD:
      return MPFR_RNDU;
    case FE_DOWNWARD:
      return MPFR_RNDD;
    }
  return -1;
}

#define prec_mpfr_f 50
#define prec_mpfr_d 80
#define prec_f 24
#define prec_d 53
#define emin_f -148
#define emin_d -1073
#define emax_f 128
#define emax_d 1024
static inline int
call_mpfr_f1 (mpfr_t y, const struct fun *f, struct args_f1 a, mpfr_rnd_t r)
{
  MPFR_DECL_INIT (x, prec_f);
  mpfr_set_flt (x, a.x, MPFR_RNDN);
  return f->fun_mpfr.f1 (y, x, r);
}
static inline int
call_mpfr_f2 (mpfr_t y, const struct fun *f, struct args_f2 a, mpfr_rnd_t r)
{
  MPFR_DECL_INIT (x, prec_f);
  MPFR_DECL_INIT (x2, prec_f);
  mpfr_set_flt (x, a.x, MPFR_RNDN);
  mpfr_set_flt (x2, a.x2, MPFR_RNDN);
  return f->fun_mpfr.f2 (y, x, x2, r);
}
static inline int
call_mpfr_d1 (mpfr_t y, const struct fun *f, struct args_d1 a, mpfr_rnd_t r)
{
  MPFR_DECL_INIT (x, prec_d);
  mpfr_set_d (x, a.x, MPFR_RNDN);
  return f->fun_mpfr.d1 (y, x, r);
}
static inline int
call_mpfr_d2 (mpfr_t y, const struct fun *f, struct args_d2 a, mpfr_rnd_t r)
{
  MPFR_DECL_INIT (x, prec_d);
  MPFR_DECL_INIT (x2, prec_d);
  mpfr_set_d (x, a.x, MPFR_RNDN);
  mpfr_set_d (x2, a.x2, MPFR_RNDN);
  return f->fun_mpfr.d2 (y, x, x2, r);
}
#endif

#define float_f float
#define double_f double
#define copysign_f copysignf
#define nextafter_f nextafterf
#define fabs_f fabsf
#define asuint_f asuint
#define asfloat_f asfloat
#define scalbn_f scalbnf
#define lscalbn_f scalbn
#define halfinf_f 0x1p127f
#define min_normal_f 0x1p-126f

#define float_d double
#define double_d long double
#define copysign_d copysign
#define nextafter_d nextafter
#define fabs_d fabs
#define asuint_d asuint64
#define asfloat_d asdouble
#define scalbn_d scalbn
#define lscalbn_d scalbnl
#define halfinf_d 0x1p1023
#define min_normal_d 0x1p-1022

#define NEW_RT
#define RT(x) x##_f
#define T(x) x##_f1
#include "ulp.h"
#undef T
#define T(x) x##_f2
#include "ulp.h"
#undef T
#undef RT

#define NEW_RT
#define RT(x) x##_d
#define T(x) x##_d1
#include "ulp.h"
#undef T
#define T(x) x##_d2
#include "ulp.h"
#undef T
#undef RT

static void
usage (void)
{
  puts ("./ulp [-q] [-m] [-f] [-r {n|u|d|z}] [-l soft-ulplimit] [-e ulplimit] func "
	"lo [hi [x lo2 hi2] [count]]");
  puts ("Compares func against a higher precision implementation in [lo; hi].");
  puts ("-q: quiet.");
  puts ("-m: use mpfr even if faster method is available.");
  puts ("-f: disable fenv exceptions testing.");
#ifdef ___vpcs
  puts ("-c: neutral 'control value' to test behaviour when one lane can affect another. \n"
	"    This should be different from tested input in other lanes, and non-special \n"
	"    (i.e. should not trigger fenv exceptions). Default is 1.");
#endif
#if WANT_SVE_TESTS
  puts ("-p: integer input for controlling predicate passed to SVE function. "
	"If bit N is set, lane N is activated (bits past the vector length "
	"are ignored). Default is UINT64_MAX (ptrue).");
#endif
  puts ("-z: ignore sign of 0.");
  puts ("Supported func:");
  for (const struct fun *f = fun; f->name; f++)
    printf ("\t%s\n", f->name);
  exit (1);
}

static int
cmp (const struct fun *f, struct gen *gen, const struct conf *conf)
{
  int r = 1;
  if (f->arity == 1 && f->singleprec)
    r = cmp_f1 (f, gen, conf);
  else if (f->arity == 2 && f->singleprec)
    r = cmp_f2 (f, gen, conf);
  else if (f->arity == 1 && !f->singleprec)
    r = cmp_d1 (f, gen, conf);
  else if (f->arity == 2 && !f->singleprec)
    r = cmp_d2 (f, gen, conf);
  else
    usage ();
  return r;
}

static uint64_t
getnum (const char *s, int singleprec)
{
  //	int i;
  uint64_t sign = 0;
  //	char buf[12];

  if (s[0] == '+')
    s++;
  else if (s[0] == '-')
    {
      sign = singleprec ? 1ULL << 31 : 1ULL << 63;
      s++;
    }

  /* Sentinel value for failed parse.  */
  char *should_not_be_s = NULL;

  /* 0xXXXX is treated as bit representation, '-' flips the sign bit.  */
  if (s[0] == '0' && tolower (s[1]) == 'x' && strchr (s, 'p') == 0)
    {
      uint64_t out = sign ^ strtoull (s, &should_not_be_s, 0);
      if (should_not_be_s == s)
	{
	  printf ("ERROR: Could not parse '%s'\n", s);
	  exit (1);
	}
      return out;
    }
  //	/* SNaN, QNaN, NaN, Inf.  */
  //	for (i=0; s[i] && i < sizeof buf; i++)
  //		buf[i] = tolower(s[i]);
  //	buf[i] = 0;
  //	if (strcmp(buf, "snan") == 0)
  //		return sign | (singleprec ? 0x7fa00000 : 0x7ff4000000000000);
  //	if (strcmp(buf, "qnan") == 0 || strcmp(buf, "nan") == 0)
  //		return sign | (singleprec ? 0x7fc00000 : 0x7ff8000000000000);
  //	if (strcmp(buf, "inf") == 0 || strcmp(buf, "infinity") == 0)
  //		return sign | (singleprec ? 0x7f800000 : 0x7ff0000000000000);
  /* Otherwise assume it's a floating-point literal.  */
  uint64_t out = sign
		 | (singleprec ? asuint (strtof (s, &should_not_be_s))
			       : asuint64 (strtod (s, &should_not_be_s)));
  if (should_not_be_s == s)
    {
      printf ("ERROR: Could not parse '%s'\n", s);
      exit (1);
    }

  return out;
}

static void
parsegen (struct gen *g, int argc, char *argv[], const struct fun *f)
{
  int singleprec = f->singleprec;
  int arity = f->arity;
  uint64_t a, b, a2, b2, n;
  if (argc < 1)
    usage ();
  b = a = getnum (argv[0], singleprec);
  n = 0;
  if (argc > 1 && strcmp (argv[1], "x") == 0)
    {
      argc -= 2;
      argv += 2;
    }
  else if (argc > 1)
    {
      b = getnum (argv[1], singleprec);
      if (argc > 2 && strcmp (argv[2], "x") == 0)
	{
	  argc -= 3;
	  argv += 3;
	}
    }
  b2 = a2 = getnum (argv[0], singleprec);
  if (argc > 1)
    b2 = getnum (argv[1], singleprec);
  if (argc > 2)
    n = strtoull (argv[2], 0, 0);
  if (argc > 3)
    usage ();
  //printf("ab %lx %lx ab2 %lx %lx n %lu\n", a, b, a2, b2, n);
  if (arity == 1)
    {
      g->start = a;
      g->len = b - a;
      if (n - 1 > b - a)
	n = b - a + 1;
      g->off = 0;
      g->step = n ? (g->len + 1) / n : 1;
      g->start2 = g->len2 = 0;
      g->cnt = n;
    }
  else if (arity == 2)
    {
      g->start = a;
      g->len = b - a;
      g->off = g->step = 0;
      g->start2 = a2;
      g->len2 = b2 - a2;
      g->cnt = n;
    }
  else
    usage ();
}

int
main (int argc, char *argv[])
{
  const struct fun *f;
  struct gen gen;
  struct conf conf;
  conf.rc = 'n';
  conf.quiet = 0;
  conf.mpfr = 0;
  conf.fenv = 1;
  conf.softlim = 0;
  conf.errlim = INFINITY;
  conf.ignore_zero_sign = 0;
#if WANT_SVE_TESTS
  uint64_t pg_int = UINT64_MAX;
#endif
  for (;;)
    {
      argc--;
      argv++;
      if (argc < 1)
	usage ();
      if (argv[0][0] != '-')
	break;
      switch (argv[0][1])
	{
	case 'e':
	  argc--;
	  argv++;
	  if (argc < 1)
	    usage ();
	  conf.errlim = strtod (argv[0], 0);
	  break;
	case 'f':
	  conf.fenv = 0;
	  break;
	case 'l':
	  argc--;
	  argv++;
	  if (argc < 1)
	    usage ();
	  conf.softlim = strtod (argv[0], 0);
	  break;
	case 'm':
	  conf.mpfr = 1;
	  break;
	case 'q':
	  conf.quiet = 1;
	  break;
	case 'r':
	  conf.rc = argv[0][2];
	  if (!conf.rc)
	    {
	      argc--;
	      argv++;
	      if (argc < 1 || argv[0][1] != '\0')
		usage ();
	      conf.rc = argv[0][0];
	    }
	  break;
	case 'z':
	  conf.ignore_zero_sign = 1;
	  break;
#if  __aarch64__ && __linux__
	case 'c':
	  argc--;
	  argv++;
	  fv[0] = strtof(argv[0], 0);
	  dv[0] = strtod(argv[0], 0);
	  break;
#endif
#if WANT_SVE_TESTS
	case 'p':
	  argc--;
	  argv++;
	  pg_int = strtoull (argv[0], 0, 0);
	  break;
#endif
	default:
	  usage ();
	}
    }
  switch (conf.rc)
    {
    case 'n':
      conf.r = FE_TONEAREST;
      break;
    case 'u':
      conf.r = FE_UPWARD;
      break;
    case 'd':
      conf.r = FE_DOWNWARD;
      break;
    case 'z':
      conf.r = FE_TOWARDZERO;
      break;
    default:
      usage ();
    }
  for (f = fun; f->name; f++)
    if (strcmp (argv[0], f->name) == 0)
      break;
  if (!f->name)
    {
#ifndef __vpcs
      /* Ignore vector math functions if vector math is not supported.  */
      if (strncmp (argv[0], "_ZGVnN", 6) == 0)
	exit (0);
#endif
#if !WANT_SVE_TESTS
      if (strncmp (argv[0], "_ZGVsMxv", 8) == 0)
	exit (0);
#endif
      printf ("math function %s not supported\n", argv[0]);
      exit (1);
    }
  if (!f->singleprec && LDBL_MANT_DIG == DBL_MANT_DIG)
    conf.mpfr = 1; /* Use mpfr if long double has no extra precision.  */
  if (!USE_MPFR && conf.mpfr)
    {
      puts ("mpfr is not available.");
      return 0;
    }
  argc--;
  argv++;
  parsegen (&gen, argc, argv, f);
  conf.n = gen.cnt;
#if WANT_SVE_TESTS
  svbool_t pg = parse_pg (pg_int, f->singleprec);
  conf.pg = &pg;
#endif
  return cmp (f, &gen, &conf);
}

#if __aarch64__ && __linux__ && WANT_SVE_TESTS && defined(__clang__)
#  pragma clang attribute pop
#endif
