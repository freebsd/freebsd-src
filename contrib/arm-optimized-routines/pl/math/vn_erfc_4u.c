/*
 * AdvSIMD vector PCS variant of __v_erfc.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS PL_ALIAS (__vn_erfc, _ZGVnN2v_erfc)
#include "v_erfc_4u.c"
#endif
