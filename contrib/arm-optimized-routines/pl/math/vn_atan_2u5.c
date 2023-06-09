/*
 * AdvSIMD vector PCS variant of __v_atan.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_atan, _ZGVnN2v_atan)
#include "v_atan_2u5.c"
#endif
