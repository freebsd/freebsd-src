#/** @file
#  Compiler intrinsic for ARM compiler
#
#  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#**/
#

#include <AsmMacroLib.h>

.syntax unified

ASM_FUNC(__switch32)
    ldr     ip, [lr, #-1]
    cmp     r0, ip
    add     r0, lr, r0, lsl #2
    ldrcc   r0, [r0, #3]
    add     ip, lr, ip, lsl #2
    ldrcs   r0, [ip, #3]
    add     ip, lr, r0
    bx      ip

