#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__ucmpdi2)
  stmfd  sp!, {r4, r5, r8, lr}
  cmp  r1, r3
  mov  r8, r0
  mov  r4, r2
  mov  r5, r3
  bcc  L2
  bhi  L4
  cmp  r0, r2
  bcc  L2
  movls  r0, #1
  bls  L8
  b  L4
L2:
  mov  r0, #0
  b  L8
L4:
  mov  r0, #2
L8:
  ldmfd  sp!, {r4, r5, r8, pc}
