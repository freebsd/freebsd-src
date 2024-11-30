#------------------------------------------------------------------------------
#
# Copyright (c) 2013, ARM. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

#
#VOID
#EFIAPI
#__aeabi_llsl (
# IN  VOID    *Destination,
# IN  VOID    *Source,
# IN  UINT32  Size
# );
#
ASM_FUNC(__aeabi_llsl)
    subs     r3,r2,#0x20
    bpl      1f
    rsb      r3,r2,#0x20
    lsl      r1,r1,r2
    orr      r1,r1,r0,lsr r3
    lsl      r0,r0,r2
    bx       lr
1:
    lsl      r1,r0,r3
    mov      r0,#0
    bx       lr
