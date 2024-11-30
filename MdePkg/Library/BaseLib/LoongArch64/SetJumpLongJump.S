#------------------------------------------------------------------------------
#
# Set/Long jump for LoongArch
#
# Copyright (c) 2022, Loongson Technology Corporation Limited. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#define STORE   st.d    /* 64 bit mode regsave instruction */
#define LOAD    ld.d    /* 64 bit mode regload instruction */
#define RSIZE   8       /* 64 bit mode register size */

ASM_GLOBAL ASM_PFX(SetJump)
ASM_GLOBAL ASM_PFX(InternalLongJump)

ASM_PFX(SetJump):
  STORE   $s0, $a0, RSIZE * 0
  STORE   $s1, $a0, RSIZE * 1
  STORE   $s2, $a0, RSIZE * 2
  STORE   $s3, $a0, RSIZE * 3
  STORE   $s4, $a0, RSIZE * 4
  STORE   $s5, $a0, RSIZE * 5
  STORE   $s6, $a0, RSIZE * 6
  STORE   $s7, $a0, RSIZE * 7
  STORE   $s8, $a0, RSIZE * 8
  STORE   $sp, $a0, RSIZE * 9
  STORE   $fp, $a0, RSIZE * 10
  STORE   $ra, $a0, RSIZE * 11
  li.w    $a0, 0                    # Setjmp return
  jirl    $zero, $ra, 0

ASM_PFX(InternalLongJump):
  LOAD    $ra, $a0, RSIZE * 11
  LOAD    $s0, $a0, RSIZE * 0
  LOAD    $s1, $a0, RSIZE * 1
  LOAD    $s2, $a0, RSIZE * 2
  LOAD    $s3, $a0, RSIZE * 3
  LOAD    $s4, $a0, RSIZE * 4
  LOAD    $s5, $a0, RSIZE * 5
  LOAD    $s6, $a0, RSIZE * 6
  LOAD    $s7, $a0, RSIZE * 7
  LOAD    $s8, $a0, RSIZE * 8
  LOAD    $sp, $a0, RSIZE * 9
  LOAD    $fp, $a0, RSIZE * 10
  move    $a0, $a1
  jirl    $zero, $ra, 0
  .end
