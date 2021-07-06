/*
 * AdvSIMD vector PCS variant of __v_exp.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_exp, _ZGVnN2v_exp)
#include "v_exp.c"
#endif
