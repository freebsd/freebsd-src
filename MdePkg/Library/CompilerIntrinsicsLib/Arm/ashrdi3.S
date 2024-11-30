#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__ashrdi3)
  cmp  r2, #31
  bls  L2
  cmp  r2, #63
  subls  r2, r2, #32
  mov  ip, r1, asr #31
  movls  r2, r1, asr r2
  movhi  r2, ip
  mov  r0, r2
  mov  r1, ip
  bx  lr
L2:
  cmp  r2, #0
  rsbne  r3, r2, #32
  movne  r3, r1, asl r3
  movne  r1, r1, asr r2
  orrne  r0, r3, r0, lsr r2
  bx  lr
