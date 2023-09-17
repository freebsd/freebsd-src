/*
 * AdvSIMD vector PCS variant of __v_acoshf.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_acoshf, _ZGVnN4v_acoshf)
#include "v_acoshf_3u1.c"
#endif
