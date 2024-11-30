//------------------------------------------------------------------------------
//
// Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------

#include <AsmMacroLib.h>

//
// A pair of (unsigned) long longs is returned in {{r0, r1}, {r2, r3}},
//  the quotient in {r0, r1}, and the remainder in {r2, r3}.
//
//__value_in_regs lldiv_t
//EFIAPI
//__aeabi_ldivmod (
//  IN UINT64  Dividen
//  IN UINT64  Divisor
//  )//
//

ASM_FUNC(__aeabi_ldivmod)
    push     {r4,lr}
    asrs     r4,r1,#1
    eor      r4,r4,r3,LSR #1
    bpl      L_Test1
    rsbs     r0,r0,#0
    rsc      r1,r1,#0
L_Test1:
    tst      r3,r3
    bpl      L_Test2
    rsbs     r2,r2,#0
    rsc      r3,r3,#0
L_Test2:
    bl       ASM_PFX(__aeabi_uldivmod)
    tst      r4,#0x40000000
    beq      L_Test3
    rsbs     r0,r0,#0
    rsc      r1,r1,#0
L_Test3:
    tst      r4,#0x80000000
    beq      L_Exit
    rsbs     r2,r2,#0
    rsc      r3,r3,#0
L_Exit:
    pop      {r4,pc}



