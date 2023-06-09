/*
 * AdvSIMD vector PCS variant of __v_tanh.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_tanh, _ZGVnN2v_tanh)
#include "v_tanh_3u.c"
#endif
