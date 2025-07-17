/*
 * Benchmark support functions.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <stdint.h>
#include <time.h>

/* Fast and accurate timer returning nanoseconds.  */
static inline uint64_t
clock_get_ns (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * (uint64_t) 1000000000 + ts.tv_nsec;
}

/* Fast 32-bit random number generator.  Passing a non-zero seed
   value resets the internal state.  */
static inline uint32_t
rand32 (uint32_t seed)
{
  static uint64_t state = 0xb707be451df0bb19ULL;
  if (seed != 0)
    state = seed;
  uint32_t res = state >> 32;
  state = state * 6364136223846793005ULL + 1;
  return res;
}

/* Macros to run a benchmark BENCH using string function FN.  */
#define RUN(BENCH, FN) BENCH(#FN, FN)

#if __aarch64__
# define RUNA64(BENCH, FN) BENCH(#FN, FN)
#else
# define RUNA64(BENCH, FN)
#endif

#if __ARM_FEATURE_SVE
# define RUNSVE(BENCH, FN) BENCH(#FN, FN)
#else
# define RUNSVE(BENCH, FN)
#endif

#if WANT_MOPS
# define RUNMOPS(BENCH, FN) BENCH(#FN, FN)
#else
# define RUNMOPS(BENCH, FN)
#endif

#if __arm__
# define RUNA32(BENCH, FN) BENCH(#FN, FN)
#else
# define RUNA32(BENCH, FN)
#endif

#if __arm__ && __ARM_ARCH >= 6 && __ARM_ARCH_ISA_THUMB == 2
# define RUNT32(BENCH, FN) BENCH(#FN, FN)
#else
# define RUNT32(BENCH, FN)
#endif
