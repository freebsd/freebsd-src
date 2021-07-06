/*
 * AdvSIMD vector PCS variant of __v_pow.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_pow, _ZGVnN2vv_pow)
#include "v_pow.c"
#endif
