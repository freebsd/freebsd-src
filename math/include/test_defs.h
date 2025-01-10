/*
 * Helper macros for emitting various details about routines for consumption by
 * runulp.sh. This version of the file is for inclusion when building routines,
 * so expansions are empty - see math/test/test_defs for versions used by the
 * build system.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception.
 */

#define TEST_ULP(f, l)
#define TEST_ULP_NONNEAREST(f, l)

#define TEST_DISABLE_FENV(f)
#define TEST_DISABLE_FENV_IF_NOT(f, e)

#define TEST_INTERVAL(f, lo, hi, n)
#define TEST_SYM_INTERVAL(f, lo, hi, n)
#define TEST_INTERVAL2(f, xlo, xhi, ylo, yhi, n)

#define TEST_CONTROL_VALUE(f, c)
