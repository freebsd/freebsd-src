#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
# Module Name:
#
#   ARShiftU64.S
#
# Abstract:
#
#   64-bit arithmetic right shift function for IA-32
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMathARShiftU64)

#------------------------------------------------------------------------------
# UINT64
# EFIAPI
# InternalMathARShiftU64 (
#   IN      UINT64                    Operand,
#   IN      UINTN                     Count
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMathARShiftU64):
    movb    12(%esp), %cl
    movl    8(%esp), %eax
    cltd
    testb   $32, %cl
    jnz     L0
    movl    %eax, %edx
    movl    4(%esp), %eax
L0:    
    shrdl   %cl, %edx, %eax
    sar     %cl, %edx
    ret
