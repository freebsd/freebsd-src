/*
 * AdvSIMD vector PCS variant of __v_tanf.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_tanf, _ZGVnN4v_tanf)
#include "v_tanf_3u5.c"
#endif
