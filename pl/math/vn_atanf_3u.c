/*
 * AdvSIMD vector PCS variant of __v_atanf.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_atanf, _ZGVnN4v_atanf)
#include "v_atanf_3u.c"
#endif
