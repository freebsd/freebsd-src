/*
 * AdvSIMD vector PCS variant of __v_cosf.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_cosf, _ZGVnN4v_cosf)
#include "v_cosf.c"
#endif
