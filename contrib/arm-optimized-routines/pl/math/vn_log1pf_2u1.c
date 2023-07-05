/*
 * AdvSIMD vector PCS variant of __v_log1pf.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_log1pf, _ZGVnN4v_log1pf)
#include "v_log1pf_2u1.c"
#endif
