/*
 * AdvSIMD vector PCS variant of __v_exp2f_1u.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#include "v_exp2f_1u.c"
#endif
