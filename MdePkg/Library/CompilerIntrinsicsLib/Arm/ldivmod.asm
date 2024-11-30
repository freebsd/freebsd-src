//------------------------------------------------------------------------------
//
// Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
// Copyright (c) 2018, Pete Batard. All rights reserved.<BR>
//
// SPDX-License-Identifier: BSD-2-Clause-Patent
//
//------------------------------------------------------------------------------


    IMPORT  __aeabi_uldivmod
    EXPORT  __aeabi_ldivmod
    EXPORT  __rt_sdiv64

    AREA  s___aeabi_ldivmod, CODE, READONLY, ARM

    ARM

;
;INT64
;EFIAPI
;__rt_sdiv64 (
;  IN  INT64  Divisor
;  IN  INT64  Dividend
;  );
;
__rt_sdiv64
    ; Swap r0-r1 and r2-r3
    MOV     r12, r0
    MOV     r0, r2
    MOV     r2, r12
    MOV     r12, r1
    MOV     r1, r3
    MOV     r3, r12
    B       __aeabi_ldivmod

;
;INT64
;EFIAPI
;__aeabi_ldivmod (
;  IN  INT64  Dividend
;  IN  INT64  Divisor
;  );
;
__aeabi_ldivmod
    PUSH     {r4,lr}
    ASRS     r4,r1,#1
    EOR      r4,r4,r3,LSR #1
    BPL      L_Test1
    RSBS     r0,r0,#0
    RSC      r1,r1,#0
L_Test1
    TST      r3,r3
    BPL      L_Test2
    RSBS     r2,r2,#0
    RSC      r3,r3,#0
L_Test2
    BL       __aeabi_uldivmod
    TST      r4,#0x40000000
    BEQ      L_Test3
    RSBS     r0,r0,#0
    RSC      r1,r1,#0
L_Test3
    TST      r4,#0x80000000
    BEQ      L_Exit
    RSBS     r2,r2,#0
    RSC      r3,r3,#0
L_Exit
    POP      {r4,pc}

    END
