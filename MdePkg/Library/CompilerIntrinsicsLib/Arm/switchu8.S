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

ASM_FUNC(__switchu8)
    ldrb      ip,[lr,#-1]
    cmp       r0,ip
    ldrbcc    r0,[lr,r0]
    ldrbcs    r0,[lr,ip]
    add       ip,lr,r0,LSL #1
    bx        ip

