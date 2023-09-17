/*
 * AdvSIMD vector PCS variant of __v_expf.
 *
 * Copyright (c) 2019, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_expf, _ZGVnN4v_expf)
#include "v_expf.c"
#endif
