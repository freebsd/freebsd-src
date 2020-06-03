//------------------------------------------------------------------------------
//
// Set/Long jump for RISC-V
//
// Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------
# define REG_S  sd
# define REG_L  ld
# define SZREG  8
.align 3
    .globl  SetJump

SetJump:
    REG_S ra,  0*SZREG(a0)
    REG_S s0,  1*SZREG(a0)
    REG_S s1,  2*SZREG(a0)
    REG_S s2,  3*SZREG(a0)
    REG_S s3,  4*SZREG(a0)
    REG_S s4,  5*SZREG(a0)
    REG_S s5,  6*SZREG(a0)
    REG_S s6,  7*SZREG(a0)
    REG_S s7,  8*SZREG(a0)
    REG_S s8,  9*SZREG(a0)
    REG_S s9,  10*SZREG(a0)
    REG_S s10, 11*SZREG(a0)
    REG_S s11, 12*SZREG(a0)
    REG_S sp,  13*SZREG(a0)
    li    a0,  0
    ret

    .globl  InternalLongJump
InternalLongJump:
    REG_L ra,  0*SZREG(a0)
    REG_L s0,  1*SZREG(a0)
    REG_L s1,  2*SZREG(a0)
    REG_L s2,  3*SZREG(a0)
    REG_L s3,  4*SZREG(a0)
    REG_L s4,  5*SZREG(a0)
    REG_L s5,  6*SZREG(a0)
    REG_L s6,  7*SZREG(a0)
    REG_L s7,  8*SZREG(a0)
    REG_L s8,  9*SZREG(a0)
    REG_L s9,  10*SZREG(a0)
    REG_L s10, 11*SZREG(a0)
    REG_L s11, 12*SZREG(a0)
    REG_L sp,  13*SZREG(a0)

    add   a0, s0, 0
    add   a1, s1, 0
    add   a2, s2, 0
    add   a3, s3, 0
    ret
