/*
 * AdvSIMD vector PCS variant of __v_sinf.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_sinf, _ZGVnN4v_sinf)
#include "v_sinf.c"
#endif
