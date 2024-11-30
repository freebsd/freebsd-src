#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

ASM_FUNC(__divsi3)
  eor  r3, r0, r0, asr #31
  eor  r2, r1, r1, asr #31
  stmfd  sp!, {r4, r5, r7, lr}
  mov  r5, r0, asr #31
  add  r7, sp, #8
  mov  r4, r1, asr #31
  sub  r0, r3, r0, asr #31
  sub  r1, r2, r1, asr #31
  bl  ASM_PFX(__udivsi3)
  eor  r1, r5, r4
  eor  r0, r0, r1
  rsb  r0, r1, r0
  ldmfd  sp!, {r4, r5, r7, pc}
