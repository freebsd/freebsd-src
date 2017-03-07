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
#   RRotU64.S
#
# Abstract:
#
#   64-bit right rotation for Ia32
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMathRRotU64)

#------------------------------------------------------------------------------
# UINT64
# EFIAPI
# InternalMathRRotU64 (
#   IN      UINT64                    Operand,
#   IN      UINTN                     Count
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMathRRotU64):
    push    %ebx
    movb    16(%esp), %cl
    movl    8(%esp), %eax
    movl    12(%esp), %edx
    shrdl   %cl, %eax, %ebx
    shrdl   %cl, %edx, %eax
    roll    %cl, %ebx
    shrdl   %cl, %ebx, %edx
    testb   $32, %cl                    # Count >= 32?
    jz      L0
    movl    %eax, %ecx                  # switch eax & edx if Count >= 32
    movl    %edx, %eax
    movl    %ecx, %edx
L0:    
    pop     %ebx
    ret
