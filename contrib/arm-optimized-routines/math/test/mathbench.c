/*
 * Microbenchmark for math functions.
 *
 * Copyright (c) 2018-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#undef _GNU_SOURCE
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "mathlib.h"

#ifndef WANT_VMATH
/* Enable the build of vector math code.  */
# define WANT_VMATH 1
#endif

/* Number of measurements, best result is reported.  */
#define MEASURE 60
/* Array size.  */
#define N 8000
/* Iterations over the array.  */
#define ITER 125

static double *Trace;
static size_t trace_size;
static double A[N];
static float Af[N];
static long measurecount = MEASURE;
static long itercount = ITER;

#if __aarch64__ && WANT_VMATH
typedef __f64x2_t v_double;

#define v_double_len() 2

static inline v_double
v_double_load (const double *p)
{
  return (v_double){p[0], p[1]};
}

static inline v_double
v_double_dup (double x)
{
  return (v_double){x, x};
}

typedef __f32x4_t v_float;

#define v_float_len() 4

static inline v_float
v_float_load (const float *p)
{
  return (v_float){p[0], p[1], p[2], p[3]};
}

static inline v_float
v_float_dup (float x)
{
  return (v_float){x, x, x, x};
}
#if WANT_SVE_MATH
#include <arm_sve.h>
typedef svbool_t sv_bool;
typedef svfloat64_t sv_double;

#define sv_double_len() svcntd()

static inline sv_double
sv_double_load (const double *p)
{
  svbool_t pg = svptrue_b64();
  return svld1(pg, p);
}

static inline sv_double
sv_double_dup (double x)
{
  return svdup_n_f64(x);
}

typedef svfloat32_t sv_float;

#define sv_float_len() svcntw()

static inline sv_float
sv_float_load (const float *p)
{
  svbool_t pg = svptrue_b32();
  return svld1(pg, p);
}

static inline sv_float
sv_float_dup (float x)
{
  return svdup_n_f32(x);
}
#endif
#else
/* dummy definitions to make things compile.  */
typedef double v_double;
typedef float v_float;
#define v_double_len(x) 1
#define v_double_load(x) (x)[0]
#define v_double_dup(x) (x)
#define v_float_len(x) 1
#define v_float_load(x) (x)[0]
#define v_float_dup(x) (x)
#endif

static double
dummy (double x)
{
  return x;
}

static float
dummyf (float x)
{
  return x;
}
#if WANT_VMATH
#if __aarch64__
static v_double
__v_dummy (v_double x)
{
  return x;
}

static v_float
__v_dummyf (v_float x)
{
  return x;
}

#ifdef __vpcs
__vpcs static v_double
__vn_dummy (v_double x)
{
  return x;
}

__vpcs static v_float
__vn_dummyf (v_float x)
{
  return x;
}
#endif
#if WANT_SVE_MATH
static sv_double
__sv_dummy (sv_double x, sv_bool pg)
{
  return x;
}

static sv_float
__sv_dummyf (sv_float x, sv_bool pg)
{
  return x;
}

#endif
#endif
#endif

#include "test/mathbench_wrappers.h"

static const struct fun
{
  const char *name;
  int prec;
  int vec;
  double lo;
  double hi;
  union
  {
    double (*d) (double);
    float (*f) (float);
    v_double (*vd) (v_double);
    v_float (*vf) (v_float);
#ifdef __vpcs
    __vpcs v_double (*vnd) (v_double);
    __vpcs v_float (*vnf) (v_float);
#endif
#if WANT_SVE_MATH
    sv_double (*svd) (sv_double, sv_bool);
    sv_float (*svf) (sv_float, sv_bool);
#endif
  } fun;
} funtab[] = {
#define D(func, lo, hi) {#func, 'd', 0, lo, hi, {.d = func}},
#define F(func, lo, hi) {#func, 'f', 0, lo, hi, {.f = func}},
#define VD(func, lo, hi) {#func, 'd', 'v', lo, hi, {.vd = func}},
#define VF(func, lo, hi) {#func, 'f', 'v', lo, hi, {.vf = func}},
#define VND(func, lo, hi) {#func, 'd', 'n', lo, hi, {.vnd = func}},
#define VNF(func, lo, hi) {#func, 'f', 'n', lo, hi, {.vnf = func}},
#define SVD(func, lo, hi) {#func, 'd', 's', lo, hi, {.svd = func}},
#define SVF(func, lo, hi) {#func, 'f', 's', lo, hi, {.svf = func}},
D (dummy, 1.0, 2.0)
F (dummyf, 1.0, 2.0)
#if WANT_VMATH
#if __aarch64__
VD (__v_dummy, 1.0, 2.0)
VF (__v_dummyf, 1.0, 2.0)
#ifdef __vpcs
VND (__vn_dummy, 1.0, 2.0)
VNF (__vn_dummyf, 1.0, 2.0)
#endif
#if WANT_SVE_MATH
SVD (__sv_dummy, 1.0, 2.0)
SVF (__sv_dummyf, 1.0, 2.0)
#endif
#endif
#endif
#include "test/mathbench_funcs.h"
{0},
#undef F
#undef D
#undef VF
#undef VD
#undef VNF
#undef VND
#undef SVF
#undef SVD
};

static void
gen_linear (double lo, double hi)
{
  for (int i = 0; i < N; i++)
    A[i] = (lo * (N - i) + hi * i) / N;
}

static void
genf_linear (double lo, double hi)
{
  for (int i = 0; i < N; i++)
    Af[i] = (float)(lo * (N - i) + hi * i) / N;
}

static inline double
asdouble (uint64_t i)
{
  union
  {
    uint64_t i;
    double f;
  } u = {i};
  return u.f;
}

static uint64_t seed = 0x0123456789abcdef;

static double
frand (double lo, double hi)
{
  seed = 6364136223846793005ULL * seed + 1;
  return lo + (hi - lo) * (asdouble (seed >> 12 | 0x3ffULL << 52) - 1.0);
}

static void
gen_rand (double lo, double hi)
{
  for (int i = 0; i < N; i++)
    A[i] = frand (lo, hi);
}

static void
genf_rand (double lo, double hi)
{
  for (int i = 0; i < N; i++)
    Af[i] = (float)frand (lo, hi);
}

static void
gen_trace (int index)
{
  for (int i = 0; i < N; i++)
    A[i] = Trace[index + i];
}

static void
genf_trace (int index)
{
  for (int i = 0; i < N; i++)
    Af[i] = (float)Trace[index + i];
}

static void
run_thruput (double f (double))
{
  for (int i = 0; i < N; i++)
    f (A[i]);
}

static void
runf_thruput (float f (float))
{
  for (int i = 0; i < N; i++)
    f (Af[i]);
}

volatile double zero = 0;

static void
run_latency (double f (double))
{
  double z = zero;
  double prev = z;
  for (int i = 0; i < N; i++)
    prev = f (A[i] + prev * z);
}

static void
runf_latency (float f (float))
{
  float z = (float)zero;
  float prev = z;
  for (int i = 0; i < N; i++)
    prev = f (Af[i] + prev * z);
}

static void
run_v_thruput (v_double f (v_double))
{
  for (int i = 0; i < N; i += v_double_len ())
    f (v_double_load (A+i));
}

static void
runf_v_thruput (v_float f (v_float))
{
  for (int i = 0; i < N; i += v_float_len ())
    f (v_float_load (Af+i));
}

static void
run_v_latency (v_double f (v_double))
{
  v_double z = v_double_dup (zero);
  v_double prev = z;
  for (int i = 0; i < N; i += v_double_len ())
    prev = f (v_double_load (A+i) + prev * z);
}

static void
runf_v_latency (v_float f (v_float))
{
  v_float z = v_float_dup (zero);
  v_float prev = z;
  for (int i = 0; i < N; i += v_float_len ())
    prev = f (v_float_load (Af+i) + prev * z);
}

#ifdef __vpcs
static void
run_vn_thruput (__vpcs v_double f (v_double))
{
  for (int i = 0; i < N; i += v_double_len ())
    f (v_double_load (A+i));
}

static void
runf_vn_thruput (__vpcs v_float f (v_float))
{
  for (int i = 0; i < N; i += v_float_len ())
    f (v_float_load (Af+i));
}

static void
run_vn_latency (__vpcs v_double f (v_double))
{
  v_double z = v_double_dup (zero);
  v_double prev = z;
  for (int i = 0; i < N; i += v_double_len ())
    prev = f (v_double_load (A+i) + prev * z);
}

static void
runf_vn_latency (__vpcs v_float f (v_float))
{
  v_float z = v_float_dup (zero);
  v_float prev = z;
  for (int i = 0; i < N; i += v_float_len ())
    prev = f (v_float_load (Af+i) + prev * z);
}
#endif

#if WANT_SVE_MATH
static void
run_sv_thruput (sv_double f (sv_double, sv_bool))
{
  for (int i = 0; i < N; i += sv_double_len ())
    f (sv_double_load (A+i), svptrue_b64 ());
}

static void
runf_sv_thruput (sv_float f (sv_float, sv_bool))
{
  for (int i = 0; i < N; i += sv_float_len ())
    f (sv_float_load (Af+i), svptrue_b32 ());
}

static void
run_sv_latency (sv_double f (sv_double, sv_bool))
{
  sv_double z = sv_double_dup (zero);
  sv_double prev = z;
  for (int i = 0; i < N; i += sv_double_len ())
    prev = f (svmad_f64_x (svptrue_b64 (), prev, z, sv_double_load (A+i)), svptrue_b64 ());
}

static void
runf_sv_latency (sv_float f (sv_float, sv_bool))
{
  sv_float z = sv_float_dup (zero);
  sv_float prev = z;
  for (int i = 0; i < N; i += sv_float_len ())
    prev = f (svmad_f32_x (svptrue_b32 (), prev, z, sv_float_load (Af+i)), svptrue_b32 ());
}
#endif

static uint64_t
tic (void)
{
  struct timespec ts;
  if (clock_gettime (CLOCK_REALTIME, &ts))
    abort ();
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

#define TIMEIT(run, f) do { \
  dt = -1; \
  run (f); /* Warm up.  */ \
  for (int j = 0; j < measurecount; j++) \
    { \
      uint64_t t0 = tic (); \
      for (int i = 0; i < itercount; i++) \
	run (f); \
      uint64_t t1 = tic (); \
      if (t1 - t0 < dt) \
	dt = t1 - t0; \
    } \
} while (0)

static void
bench1 (const struct fun *f, int type, double lo, double hi)
{
  uint64_t dt = 0;
  uint64_t ns100;
  const char *s = type == 't' ? "rthruput" : "latency";
  int vlen = 1;

  if (f->vec && f->prec == 'd')
    vlen = v_double_len();
  else if (f->vec && f->prec == 'f')
    vlen = v_float_len();

  if (f->prec == 'd' && type == 't' && f->vec == 0)
    TIMEIT (run_thruput, f->fun.d);
  else if (f->prec == 'd' && type == 'l' && f->vec == 0)
    TIMEIT (run_latency, f->fun.d);
  else if (f->prec == 'f' && type == 't' && f->vec == 0)
    TIMEIT (runf_thruput, f->fun.f);
  else if (f->prec == 'f' && type == 'l' && f->vec == 0)
    TIMEIT (runf_latency, f->fun.f);
  else if (f->prec == 'd' && type == 't' && f->vec == 'v')
    TIMEIT (run_v_thruput, f->fun.vd);
  else if (f->prec == 'd' && type == 'l' && f->vec == 'v')
    TIMEIT (run_v_latency, f->fun.vd);
  else if (f->prec == 'f' && type == 't' && f->vec == 'v')
    TIMEIT (runf_v_thruput, f->fun.vf);
  else if (f->prec == 'f' && type == 'l' && f->vec == 'v')
    TIMEIT (runf_v_latency, f->fun.vf);
#ifdef __vpcs
  else if (f->prec == 'd' && type == 't' && f->vec == 'n')
    TIMEIT (run_vn_thruput, f->fun.vnd);
  else if (f->prec == 'd' && type == 'l' && f->vec == 'n')
    TIMEIT (run_vn_latency, f->fun.vnd);
  else if (f->prec == 'f' && type == 't' && f->vec == 'n')
    TIMEIT (runf_vn_thruput, f->fun.vnf);
  else if (f->prec == 'f' && type == 'l' && f->vec == 'n')
    TIMEIT (runf_vn_latency, f->fun.vnf);
#endif
#if WANT_SVE_MATH
  else if (f->prec == 'd' && type == 't' && f->vec == 's')
    TIMEIT (run_sv_thruput, f->fun.svd);
  else if (f->prec == 'd' && type == 'l' && f->vec == 's')
    TIMEIT (run_sv_latency, f->fun.svd);
  else if (f->prec == 'f' && type == 't' && f->vec == 's')
    TIMEIT (runf_sv_thruput, f->fun.svf);
  else if (f->prec == 'f' && type == 'l' && f->vec == 's')
    TIMEIT (runf_sv_latency, f->fun.svf);
#endif

  if (type == 't')
    {
      ns100 = (100 * dt + itercount * N / 2) / (itercount * N);
      printf ("%9s %8s: %4u.%02u ns/elem %10llu ns in [%g %g]\n", f->name, s,
	      (unsigned) (ns100 / 100), (unsigned) (ns100 % 100),
	      (unsigned long long) dt, lo, hi);
    }
  else if (type == 'l')
    {
      ns100 = (100 * dt + itercount * N / vlen / 2) / (itercount * N / vlen);
      printf ("%9s %8s: %4u.%02u ns/call %10llu ns in [%g %g]\n", f->name, s,
	      (unsigned) (ns100 / 100), (unsigned) (ns100 % 100),
	      (unsigned long long) dt, lo, hi);
    }
  fflush (stdout);
}

static void
bench (const struct fun *f, double lo, double hi, int type, int gen)
{
  if (f->prec == 'd' && gen == 'r')
    gen_rand (lo, hi);
  else if (f->prec == 'd' && gen == 'l')
    gen_linear (lo, hi);
  else if (f->prec == 'd' && gen == 't')
    gen_trace (0);
  else if (f->prec == 'f' && gen == 'r')
    genf_rand (lo, hi);
  else if (f->prec == 'f' && gen == 'l')
    genf_linear (lo, hi);
  else if (f->prec == 'f' && gen == 't')
    genf_trace (0);

  if (gen == 't')
    hi = trace_size / N;

  if (type == 'b' || type == 't')
    bench1 (f, 't', lo, hi);

  if (type == 'b' || type == 'l')
    bench1 (f, 'l', lo, hi);

  for (int i = N; i < trace_size; i += N)
    {
      if (f->prec == 'd')
	gen_trace (i);
      else
	genf_trace (i);

      lo = i / N;
      if (type == 'b' || type == 't')
	bench1 (f, 't', lo, hi);

      if (type == 'b' || type == 'l')
	bench1 (f, 'l', lo, hi);
    }
}

static void
readtrace (const char *name)
{
	int n = 0;
	FILE *f = strcmp (name, "-") == 0 ? stdin : fopen (name, "r");
	if (!f)
	  {
	    printf ("openning \"%s\" failed: %m\n", name);
	    exit (1);
	  }
	for (;;)
	  {
	    if (n >= trace_size)
	      {
		trace_size += N;
		Trace = realloc (Trace, trace_size * sizeof (Trace[0]));
		if (Trace == NULL)
		  {
		    printf ("out of memory\n");
		    exit (1);
		  }
	      }
	    if (fscanf (f, "%lf", Trace + n) != 1)
	      break;
	    n++;
	  }
	if (ferror (f) || n == 0)
	  {
	    printf ("reading \"%s\" failed: %m\n", name);
	    exit (1);
	  }
	fclose (f);
	if (n % N == 0)
	  trace_size = n;
	for (int i = 0; n < trace_size; n++, i++)
	  Trace[n] = Trace[i];
}

static void
usage (void)
{
  printf ("usage: ./mathbench [-g rand|linear|trace] [-t latency|thruput|both] "
	  "[-i low high] [-f tracefile] [-m measurements] [-c iterations] func "
	  "[func2 ..]\n");
  printf ("func:\n");
  printf ("%7s [run all benchmarks]\n", "all");
  for (const struct fun *f = funtab; f->name; f++)
    printf ("%7s [low: %g high: %g]\n", f->name, f->lo, f->hi);
  exit (1);
}

int
main (int argc, char *argv[])
{
  int usergen = 0, gen = 'r', type = 'b', all = 0;
  double lo = 0, hi = 0;
  const char *tracefile = "-";

  argv++;
  argc--;
  for (;;)
    {
      if (argc <= 0)
	usage ();
      if (argv[0][0] != '-')
	break;
      else if (argc >= 3 && strcmp (argv[0], "-i") == 0)
	{
	  usergen = 1;
	  lo = strtod (argv[1], 0);
	  hi = strtod (argv[2], 0);
	  argv += 3;
	  argc -= 3;
	}
      else if (argc >= 2 && strcmp (argv[0], "-m") == 0)
	{
	  measurecount = strtol (argv[1], 0, 0);
	  argv += 2;
	  argc -= 2;
	}
      else if (argc >= 2 && strcmp (argv[0], "-c") == 0)
	{
	  itercount = strtol (argv[1], 0, 0);
	  argv += 2;
	  argc -= 2;
	}
      else if (argc >= 2 && strcmp (argv[0], "-g") == 0)
	{
	  gen = argv[1][0];
	  if (strchr ("rlt", gen) == 0)
	    usage ();
	  argv += 2;
	  argc -= 2;
	}
      else if (argc >= 2 && strcmp (argv[0], "-f") == 0)
	{
	  gen = 't';  /* -f implies -g trace.  */
	  tracefile = argv[1];
	  argv += 2;
	  argc -= 2;
	}
      else if (argc >= 2 && strcmp (argv[0], "-t") == 0)
	{
	  type = argv[1][0];
	  if (strchr ("ltb", type) == 0)
	    usage ();
	  argv += 2;
	  argc -= 2;
	}
      else
	usage ();
    }
  if (gen == 't')
    {
      readtrace (tracefile);
      lo = hi = 0;
      usergen = 1;
    }
  while (argc > 0)
    {
      int found = 0;
      all = strcmp (argv[0], "all") == 0;
      for (const struct fun *f = funtab; f->name; f++)
	if (all || strcmp (argv[0], f->name) == 0)
	  {
	    found = 1;
	    if (!usergen)
	      {
		lo = f->lo;
		hi = f->hi;
	      }
	    bench (f, lo, hi, type, gen);
	    if (usergen && !all)
	      break;
	  }
      if (!found)
	printf ("unknown function: %s\n", argv[0]);
      argv++;
      argc--;
    }
  return 0;
}
