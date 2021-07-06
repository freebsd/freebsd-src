/*
 * AdvSIMD vector PCS variant of __v_expf_1u.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#include "v_expf_1u.c"
#endif
