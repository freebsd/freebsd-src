/*
 * types.h
 *
 * Copyright (c) 2005-2019, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef mathtest_types_h
#define mathtest_types_h

#include <limits.h>

#if UINT_MAX == 4294967295
typedef unsigned int uint32;
typedef int int32;
#define I32 ""
#elif ULONG_MAX == 4294967295
typedef unsigned long uint32;
typedef long int32;
#define I32 "l"
#else
#error Could not find an unsigned 32-bit integer type
#endif

#endif
