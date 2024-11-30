#------------------------------------------------------------------------------
#
# Copyright (c) 2013, ARM. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

#VOID
#EFIAPI
#__aeabi_llsr (
# IN  VOID    *Destination,
# IN  VOID    *Source,
# IN  UINT32  Size
# );
#
ASM_FUNC(__aeabi_llsr)
    subs     r3,r2,#0x20
    bpl      1f
    rsb      r3,r2,#0x20
    lsr      r0,r0,r2
    orr      r0,r0,r1,lsl r3
    lsr      r1,r1,r2
    bx       lr
1:
    lsr      r0,r1,r3
    mov      r1,#0
    bx       lr
