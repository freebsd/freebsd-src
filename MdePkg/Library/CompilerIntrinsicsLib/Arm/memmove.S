#------------------------------------------------------------------------------
#
# Copyright (c) 2011-2014, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

# VOID
# EFIAPI
# memmove (
#  IN  VOID          *Destination,
#  IN  CONST VOID    *Source,
#  IN  UINT32        Size
#  );
ASM_FUNC(memmove)
  CMP     r2, #0
  BXEQ    lr
  CMP     r0, r1
  BXEQ    lr
  BHI     memmove_backward

memmove_forward:
  LDRB    r3, [r1], #1
  STRB    r3, [r0], #1
  SUBS    r2, r2, #1
  BXEQ    lr
  B       memmove_forward

memmove_backward:
  add     r0, r2
  add     r1, r2
memmove_backward_loop:
  LDRB    r3, [r1, #-1]!
  STRB    r3, [r0, #-1]!
  SUBS    r2, r2, #1
  BXEQ    lr
  B       memmove_backward_loop
