#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

#
#UINT32
#EFIAPI
#__aeabi_uwrite4 (
#  IN UINT32 Data,
#  IN VOID   *Pointer
#  );
#
ASM_FUNC(__aeabi_uwrite4)
    mov     r2, r0, lsr #8
    strb    r0, [r1]
    strb    r2, [r1, #1]
    mov     r2, r0, lsr #16
    strb    r2, [r1, #2]
    mov     r2, r0, lsr #24
    strb    r2, [r1, #3]
    bx      lr

#
#UINT64
#EFIAPI
#__aeabi_uwrite8 (
#  IN UINT64 Data,
#  IN VOID   *Pointer
#  );
#
ASM_FUNC(__aeabi_uwrite8)
    mov     r3, r0, lsr #8
    strb    r0, [r2]
    strb    r3, [r2, #1]
    mov     r3, r0, lsr #16
    strb    r3, [r2, #2]
    mov     r3, r0, lsr #24
    strb    r3, [r2, #3]

    mov     r3, r1, lsr #8
    strb    r1, [r2, #4]
    strb    r3, [r2, #5]
    mov     r3, r1, lsr #16
    strb    r3, [r2, #6]
    mov     r3, r1, lsr #24
    strb    r3, [r2, #7]
    bx      lr
