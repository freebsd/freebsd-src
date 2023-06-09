/*
 * AdvSIMD vector PCS variant of __v_tanhf.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_tanhf, _ZGVnN4v_tanhf)
#include "v_tanhf_2u6.c"
#endif
