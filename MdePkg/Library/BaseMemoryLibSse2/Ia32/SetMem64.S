#------------------------------------------------------------------------------
#
# Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
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
#   SetMem64.S
#
# Abstract:
#
#   SetMem64 function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemSetMem64)

#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemSetMem64 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT64 Value
#    );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemSetMem64):
    movl    4(%esp), %eax
    movl    8(%esp), %ecx
    testb   $8, %al
    movl    %eax, %edx
    movq    0xc(%esp), %xmm0
    jz      L1
    movq    %xmm0, (%edx)
    addl    $8, %edx
    decl    %ecx
L1:
    shrl    %ecx
    jz      L_SetQwords
    movlhps %xmm0, %xmm0
L2:
    movntdq %xmm0, (%edx)
    leal    16(%edx), %edx
    loop    L2
    mfence
L_SetQwords:
    jnc     L3
    movq    %xmm0, (%edx)
L3:
    ret
