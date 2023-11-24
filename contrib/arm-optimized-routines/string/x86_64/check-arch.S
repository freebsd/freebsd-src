/*
 * check ARCH setting.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if !__x86_64__
# error ARCH setting does not match the compiler.
#endif
