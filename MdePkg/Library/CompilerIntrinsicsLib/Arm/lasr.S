#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

#
#UINT64
#EFIAPI
#__aeabi_lasr (
#  IN UINT64  Value
#  IN UINT32  Shift
#  );
#
ASM_FUNC(__aeabi_lasr)
  subs     r3,r2,#0x20
  bpl      L_Test
  rsb      r3,r2,#0x20
  lsr      r0,r0,r2
  orr      r0,r0,r1,LSL r3
  asr      r1,r1,r2
  bx       lr
L_Test:
  asr      r0,r1,r3
  asr      r1,r1,#31
  bx       lr
