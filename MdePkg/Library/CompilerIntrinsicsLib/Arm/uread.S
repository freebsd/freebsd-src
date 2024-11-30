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
#__aeabi_uread4 (
#  IN VOID   *Pointer
#  );
#
ASM_FUNC(__aeabi_uread4)
    ldrb    r1, [r0]
    ldrb    r2, [r0, #1]
    ldrb    r3, [r0, #2]
    ldrb    r0, [r0, #3]
    orr     r1, r1, r2, lsl #8
    orr     r1, r1, r3, lsl #16
    orr     r0, r1, r0, lsl #24
    bx      lr

#
#UINT64
#EFIAPI
#__aeabi_uread8 (
#  IN VOID   *Pointer
#  );
#
ASM_FUNC(__aeabi_uread8)
    mov     r3, r0

    ldrb    r1, [r3]
    ldrb    r2, [r3, #1]
    orr     r1, r1, r2, lsl #8
    ldrb    r2, [r3, #2]
    orr     r1, r1, r2, lsl #16
    ldrb    r0, [r3, #3]
    orr     r0, r1, r0, lsl #24

    ldrb    r1, [r3, #4]
    ldrb    r2, [r3, #5]
    orr     r1, r1, r2, lsl #8
    ldrb    r2, [r3, #6]
    orr     r1, r1, r2, lsl #16
    ldrb    r2, [r3, #7]
    orr     r1, r1, r2, lsl #24

    bx      lr
