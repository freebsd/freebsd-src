#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__umoddi3)
  stmfd  sp!, {r7, lr}
  add  r7, sp, #0
  sub  sp, sp, #16
  add  ip, sp, #8
  str  ip, [sp, #0]
  bl  ASM_PFX(__udivmoddi4)
  ldrd  r0, [sp, #8]
  sub  sp, r7, #0
  ldmfd  sp!, {r7, pc}

