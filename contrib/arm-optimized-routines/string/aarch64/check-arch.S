/*
 * check ARCH setting.
 *
 * Copyright (c) 2020-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if !__aarch64__
# error ARCH setting does not match the compiler.
#endif

/* Include for GNU property notes.  */
#include "asmdefs.h"
