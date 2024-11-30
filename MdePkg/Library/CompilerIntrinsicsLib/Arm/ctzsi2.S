#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__ctzsi2)
  uxth  r3, r0
  cmp  r3, #0
  moveq  ip, #16
  movne  ip, #0
  @ lr needed for prologue
  mov  r0, r0, lsr ip
  tst  r0, #255
  movne  r3, #0
  moveq  r3, #8
  mov  r0, r0, lsr r3
  tst  r0, #15
  movne  r1, #0
  moveq  r1, #4
  add  r3, r3, ip
  mov  r0, r0, lsr r1
  tst  r0, #3
  movne  r2, #0
  moveq  r2, #2
  add  r3, r3, r1
  mov  r0, r0, lsr r2
  and  r0, r0, #3
  add  r2, r3, r2
  eor  r3, r0, #1
  mov  r0, r0, lsr #1
  ands  r3, r3, #1
  mvnne  r3, #0
  rsb  r0, r0, #2
  and  r0, r3, r0
  add  r0, r2, r0
  bx  lr
