/*
 * Constants for double-precision e^(x+tail) vector function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

#define C1_scal 0x1.fffffffffffd4p-2
#define C2_scal 0x1.5555571d6b68cp-3
#define C3_scal 0x1.5555576a59599p-5
#define InvLn2_scal 0x1.71547652b82fep8 /* N/ln2.  */
#define Ln2hi_scal 0x1.62e42fefa39efp-9 /* ln2/N.  */
#define Ln2lo_scal 0x1.abc9e3b39803f3p-64

#define N (1 << V_EXP_TAIL_TABLE_BITS)
#define Tab __v_exp_tail_data
#define IndexMask_scal (N - 1)
#define Shift_scal 0x1.8p+52
#define Thres_scal 704.0
