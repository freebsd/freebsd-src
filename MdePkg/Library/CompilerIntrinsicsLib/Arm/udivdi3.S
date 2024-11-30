#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__udivdi3)
  stmfd  sp!, {r7, lr}
  add  r7, sp, #0
  sub  sp, sp, #8
  mov  ip, #0
  str  ip, [sp, #0]
  bl  ASM_PFX(__udivmoddi4)
  sub  sp, r7, #0
  ldmfd  sp!, {r7, pc}
