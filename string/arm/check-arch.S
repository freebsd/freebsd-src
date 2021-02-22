/*
 * check ARCH setting.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#if !__arm__
# error ARCH setting does not match the compiler.
#endif
