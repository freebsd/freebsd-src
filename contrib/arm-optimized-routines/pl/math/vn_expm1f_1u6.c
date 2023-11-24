/*
 * AdvSIMD vector PCS variant of __v_expm1f.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_expm1f, _ZGVnN4v_expm1f)
#include "v_expm1f_1u6.c"
#endif
