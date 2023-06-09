/*
 * Helper macros for single-precision Horner polynomial evaluation.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if V_SUPPORTED
#define FMA v_fma_f64
#else
#define FMA fma
#endif

#include "horner_wrap.h"
