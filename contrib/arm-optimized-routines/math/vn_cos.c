/*
 * AdvSIMD vector PCS variant of __v_cos.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_cos, _ZGVnN2v_cos)
#include "v_cos.c"
#endif
