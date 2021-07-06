/*
 * check ARCH setting.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#if !__aarch64__
# error ARCH setting does not match the compiler.
#endif

/* Include for GNU property notes.  */
#include "../asmdefs.h"
