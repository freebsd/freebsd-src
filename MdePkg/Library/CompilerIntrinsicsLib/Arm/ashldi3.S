#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__ashldi3)
  cmp  r2, #31
  bls  L2
  cmp  r2, #63
  subls  r2, r2, #32
  movls  r2, r0, asl r2
  movhi  r2, #0
  mov  r1, r2
  mov  r0, #0
  bx  lr
L2:
  cmp  r2, #0
  rsbne  r3, r2, #32
  movne  r3, r0, lsr r3
  movne  r0, r0, asl r2
  orrne  r1, r3, r1, asl r2
  bx  lr
