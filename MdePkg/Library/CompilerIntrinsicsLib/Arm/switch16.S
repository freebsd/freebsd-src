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

ASM_FUNC(__switch16)
    ldrh      ip, [lr, #-1]
    cmp       r0, ip
    add       r0, lr, r0, lsl #1
    ldrshcc   r0, [r0, #1]
    add       ip, lr, ip, lsl #1
    ldrshcs   r0, [ip, #1]
    add       ip, lr, r0, lsl #1
    bx        ip


