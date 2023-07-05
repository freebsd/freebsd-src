/*
 * Helper macros for double-precision Horner polynomial evaluation.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if V_SUPPORTED
#define FMA v_fma_f32
#else
#define FMA fmaf
#endif

#include "horner_wrap.h"
