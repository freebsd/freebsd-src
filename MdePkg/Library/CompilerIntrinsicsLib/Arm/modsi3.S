#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__modsi3)
  stmfd  sp!, {r4, r5, r7, lr}
  add  r7, sp, #8
  mov  r5, r0
  mov  r4, r1
  bl  ASM_PFX(__divsi3)
  mul  r0, r4, r0
  rsb  r0, r0, r5
  ldmfd  sp!, {r4, r5, r7, pc}
