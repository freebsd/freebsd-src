/*
 * PL macros for emitting various details about routines for consumption by
 * runulp.sh.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception.
 */

/* Emit the max ULP threshold, l, for routine f. Piggy-back PL_TEST_EXPECT_FENV
   on PL_TEST_ULP to add EXPECT_FENV to all scalar routines.  */
#if WANT_VMATH || defined(IGNORE_SCALAR_FENV)
# define PL_TEST_ULP(f, l) PL_TEST_ULP f l
#else
# define PL_TEST_ULP(f, l)                                                   \
    PL_TEST_EXPECT_FENV_ALWAYS (f)                                            \
    PL_TEST_ULP f l
#endif

/* Emit routine name if e == 1 and f is expected to correctly trigger fenv
   exceptions. e allows declaration to be emitted conditionally upon certain
   build flags - defer expansion by one pass to allow those flags to be expanded
   properly.  */
#define PL_TEST_EXPECT_FENV(f, e) PL_TEST_EXPECT_FENV_ (f, e)
#define PL_TEST_EXPECT_FENV_(f, e) PL_TEST_EXPECT_FENV_##e (f)
#define PL_TEST_EXPECT_FENV_1(f) PL_TEST_EXPECT_FENV_ENABLED f
#define PL_TEST_EXPECT_FENV_ALWAYS(f) PL_TEST_EXPECT_FENV (f, 1)

#define PL_TEST_INTERVAL(f, lo, hi, n) PL_TEST_INTERVAL f lo hi n
#define PL_TEST_SYM_INTERVAL(f, lo, hi, n)                                    \
  PL_TEST_INTERVAL (f, lo, hi, n)                                             \
  PL_TEST_INTERVAL (f, -lo, -hi, n)
#define PL_TEST_INTERVAL_C(f, lo, hi, n, c) PL_TEST_INTERVAL f lo hi n c
#define PL_TEST_SYM_INTERVAL_C(f, lo, hi, n, c)                               \
  PL_TEST_INTERVAL_C (f, lo, hi, n, c)                                        \
  PL_TEST_INTERVAL_C (f, -lo, -hi, n, c)
// clang-format off
#define PL_TEST_INTERVAL2(f, xlo, xhi, ylo, yhi, n)                            \
  PL_TEST_INTERVAL f xlo,ylo xhi,yhi n
// clang-format on
