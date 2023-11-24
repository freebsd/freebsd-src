/*
 * PL macros to aid testing. This version of this file is used for building the
 * routine, not the tests. Separate definitions are found in test/pl_test.h
 * which emit test parameters.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception.
 */

/* Emit max ULP threshold - silenced for building the routine.  */
#define PL_TEST_ULP(f, l)

/* Emit alias. The PL_TEST_ALIAS declaration is piggy-backed on top of
   strong_alias. Use PL_ALIAS instead of strong_alias to make sure the alias is
   also added to the test suite.  */
#define PL_ALIAS(a, b) strong_alias (a, b)

/* Emit routine name if e == 1 and f is expected to correctly trigger fenv
   exceptions. e allows declaration to be emitted conditionally upon certain
   build flags - defer expansion by one pass to allow those flags to be expanded
   properly.  */
#define PL_TEST_EXPECT_FENV(f, e)
#define PL_TEST_EXPECT_FENV_ALWAYS(f)

#define PL_TEST_INTERVAL(f, lo, hi, n)
#define PL_TEST_INTERVAL_C(f, lo, hi, n, c)
