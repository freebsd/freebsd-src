/*
 * AdvSIMD vector PCS variant of __v_acosh.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_acosh, _ZGVnN2v_acosh)
#include "v_acosh_3u5.c"
#endif
