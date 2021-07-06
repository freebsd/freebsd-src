/*
 * AdvSIMD vector PCS variant of __v_sin.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_sin, _ZGVnN2v_sin)
#include "v_sin.c"
#endif
