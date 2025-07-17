/*
 * Microbenchmark for math functions.
 *
 * Copyright (c) 2018-2024, Arm Limited.
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

#undef _GNU_SOURCE
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "mathlib.h"

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
#if __aarch64__ && __linux__
__vpcs static float64x2_t
__vn_dummy (float64x2_t x)
{
  return x;
}

__vpcs static float32x4_t
__vn_dummyf (float32x4_t x)
{
  return x;
}
#endif
#if WANT_SVE_TESTS
static svfloat64_t
__sv_dummy (svfloat64_t x, svbool_t pg)
{
  return x;
}

static svfloat32_t
__sv_dummyf (svfloat32_t x, svbool_t pg)
{
  return x;
}

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
#if __aarch64__ && __linux__
    __vpcs float64x2_t (*vnd) (float64x2_t);
    __vpcs float32x4_t (*vnf) (float32x4_t);
#endif
#if WANT_SVE_TESTS
    svfloat64_t (*svd) (svfloat64_t, svbool_t);
    svfloat32_t (*svf) (svfloat32_t, svbool_t);
#endif
  } fun;
} funtab[] = {
// clang-format off
#define D(func, lo, hi) {#func, 'd', 0, lo, hi, {.d = func}},
#define F(func, lo, hi) {#func, 'f', 0, lo, hi, {.f = func}},
#define VND(func, lo, hi) {#func, 'd', 'n', lo, hi, {.vnd = func}},
#define VNF(func, lo, hi) {#func, 'f', 'n', lo, hi, {.vnf = func}},
#define SVD(func, lo, hi) {#func, 'd', 's', lo, hi, {.svd = func}},
#define SVF(func, lo, hi) {#func, 'f', 's', lo, hi, {.svf = func}},
D (dummy, 1.0, 2.0)
F (dummyf, 1.0, 2.0)
#if  __aarch64__ && __linux__
VND (__vn_dummy, 1.0, 2.0)
VNF (__vn_dummyf, 1.0, 2.0)
#endif
#if WANT_SVE_TESTS
SVD (__sv_dummy, 1.0, 2.0)
SVF (__sv_dummyf, 1.0, 2.0)
#endif
#include "test/mathbench_funcs.h"
{0},
#undef F
#undef D
#undef VNF
#undef VND
#undef SVF
#undef SVD
  // clang-format on
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

#if  __aarch64__ && __linux__
static void
run_vn_thruput (__vpcs float64x2_t f (float64x2_t))
{
  for (int i = 0; i < N; i += 2)
    f (vld1q_f64 (A + i));
}

static void
runf_vn_thruput (__vpcs float32x4_t f (float32x4_t))
{
  for (int i = 0; i < N; i += 4)
    f (vld1q_f32 (Af + i));
}

static void
run_vn_latency (__vpcs float64x2_t f (float64x2_t))
{
  volatile uint64x2_t vsel = (uint64x2_t) { 0, 0 };
  uint64x2_t sel = vsel;
  float64x2_t prev = vdupq_n_f64 (0);
  for (int i = 0; i < N; i += 2)
    prev = f (vbslq_f64 (sel, prev, vld1q_f64 (A + i)));
}

static void
runf_vn_latency (__vpcs float32x4_t f (float32x4_t))
{
  volatile uint32x4_t vsel = (uint32x4_t) { 0, 0, 0, 0 };
  uint32x4_t sel = vsel;
  float32x4_t prev = vdupq_n_f32 (0);
  for (int i = 0; i < N; i += 4)
    prev = f (vbslq_f32 (sel, prev, vld1q_f32 (Af + i)));
}
#endif

#if WANT_SVE_TESTS
static void
run_sv_thruput (svfloat64_t f (svfloat64_t, svbool_t))
{
  for (int i = 0; i < N; i += svcntd ())
    f (svld1_f64 (svptrue_b64 (), A + i), svptrue_b64 ());
}

static void
runf_sv_thruput (svfloat32_t f (svfloat32_t, svbool_t))
{
  for (int i = 0; i < N; i += svcntw ())
    f (svld1_f32 (svptrue_b32 (), Af + i), svptrue_b32 ());
}

static void
run_sv_latency (svfloat64_t f (svfloat64_t, svbool_t))
{
  volatile svbool_t vsel = svptrue_b64 ();
  svbool_t sel = vsel;
  svfloat64_t prev = svdup_f64 (0);
  for (int i = 0; i < N; i += svcntd ())
    prev = f (svsel_f64 (sel, svld1_f64 (svptrue_b64 (), A + i), prev),
	      svptrue_b64 ());
}

static void
runf_sv_latency (svfloat32_t f (svfloat32_t, svbool_t))
{
  volatile svbool_t vsel = svptrue_b32 ();
  svbool_t sel = vsel;
  svfloat32_t prev = svdup_f32 (0);
  for (int i = 0; i < N; i += svcntw ())
    prev = f (svsel_f32 (sel, svld1_f32 (svptrue_b32 (), Af + i), prev),
	      svptrue_b32 ());
}
#endif

static uint64_t
tic (void)
{
  struct timespec ts;
#if defined(_MSC_VER)
  if (!timespec_get (&ts, TIME_UTC))
#else
  if (clock_gettime (CLOCK_REALTIME, &ts))
#endif
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

  if (f->vec == 'n')
    vlen = f->prec == 'd' ? 2 : 4;
#if WANT_SVE_TESTS
  else if (f->vec == 's')
    vlen = f->prec == 'd' ? svcntd () : svcntw ();
#endif

  if (f->prec == 'd' && type == 't' && f->vec == 0)
    TIMEIT (run_thruput, f->fun.d);
  else if (f->prec == 'd' && type == 'l' && f->vec == 0)
    TIMEIT (run_latency, f->fun.d);
  else if (f->prec == 'f' && type == 't' && f->vec == 0)
    TIMEIT (runf_thruput, f->fun.f);
  else if (f->prec == 'f' && type == 'l' && f->vec == 0)
    TIMEIT (runf_latency, f->fun.f);
#if __aarch64__ && __linux__
  else if (f->prec == 'd' && type == 't' && f->vec == 'n')
    TIMEIT (run_vn_thruput, f->fun.vnd);
  else if (f->prec == 'd' && type == 'l' && f->vec == 'n')
    TIMEIT (run_vn_latency, f->fun.vnd);
  else if (f->prec == 'f' && type == 't' && f->vec == 'n')
    TIMEIT (runf_vn_thruput, f->fun.vnf);
  else if (f->prec == 'f' && type == 'l' && f->vec == 'n')
    TIMEIT (runf_vn_latency, f->fun.vnf);
#endif
#if WANT_SVE_TESTS
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
      printf ("%9s %8s: %4u.%02u ns/elem %10llu ns in [%g %g] vlen %d\n",
	      f->name, s,
	      (unsigned) (ns100 / 100), (unsigned) (ns100 % 100),
	      (unsigned long long) dt, lo, hi, vlen);
    }
  else if (type == 'l')
    {
      ns100 = (100 * dt + itercount * N / vlen / 2) / (itercount * N / vlen);
      printf ("%9s %8s: %4u.%02u ns/call %10llu ns in [%g %g] vlen %d\n",
	      f->name, s,
	      (unsigned) (ns100 / 100), (unsigned) (ns100 % 100),
	      (unsigned long long) dt, lo, hi, vlen);
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

#if __aarch64__ && __linux__ && WANT_SVE_TESTS && defined(__clang__)
#  pragma clang attribute pop
#endif
