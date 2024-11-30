#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__clzsi2)
  @ frame_needed = 1, uses_anonymous_args = 0
  stmfd  sp!, {r7, lr}
  add  r7, sp, #0
  movs  r3, r0, lsr #16
  movne  r3, #16
  moveq  r3, #0
  movne  r9, #0
  moveq  r9, #16
  mov  r3, r0, lsr r3
  tst  r3, #65280
  movne  r0, #8
  moveq  r0, #0
  movne  lr, #0
  moveq  lr, #8
  mov  r3, r3, lsr r0
  tst  r3, #240
  movne  r0, #4
  moveq  r0, #0
  movne  ip, #0
  moveq  ip, #4
  mov  r3, r3, lsr r0
  tst  r3, #12
  movne  r0, #2
  moveq  r0, #0
  movne  r1, #0
  moveq  r1, #2
  mov  r2, r3, lsr r0
  add  r3, lr, r9
  add  r0, r3, ip
  add  r1, r0, r1
  mov  r0, r2, lsr #1
  eor  r0, r0, #1
  ands  r0, r0, #1
  mvnne  r0, #0
  rsb  r3, r2, #2
  and  r0, r0, r3
  add  r0, r1, r0
  ldmfd  sp!, {r7, pc}
