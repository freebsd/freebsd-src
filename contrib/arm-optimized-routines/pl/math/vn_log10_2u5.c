/*
 * AdvSIMD vector PCS variant of __v_log10.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_log10, _ZGVnN2v_log10)
#include "v_log10_2u5.c"
#endif
