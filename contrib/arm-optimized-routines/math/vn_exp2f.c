/*
 * AdvSIMD vector PCS variant of __v_exp2f.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_exp2f, _ZGVnN4v_exp2f)
#include "v_exp2f.c"
#endif
