/*
 * check ARCH setting.
 *
 * Copyright (c) 2020-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if !__arm__
# error ARCH setting does not match the compiler.
#endif

/* For attributes that may affect ABI.  */
#include "asmdefs.h"
