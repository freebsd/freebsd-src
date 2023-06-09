/*
 * AdvSIMD vector PCS variant of __v_log2f.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_log2f, _ZGVnN4v_log2f)
#include "v_log2f_2u5.c"
#endif
