/*
 * AdvSIMD vector PCS variant of __v_log.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_log, _ZGVnN2v_log)
#include "v_log.c"
#endif
