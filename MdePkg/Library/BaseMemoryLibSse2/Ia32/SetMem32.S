#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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
#   SetMem32.asm
#
# Abstract:
#
#   SetMem32 function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemSetMem32)

#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemSetMem32 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT32 Value
#    );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemSetMem32):
    push    %edi
    movl    12(%esp), %edx
    movl    8(%esp), %edi
    xorl    %ecx, %ecx
    subl    %edi, %ecx
    andl    $15, %ecx                   # ecx + edi aligns on 16-byte boundary
    movl    16(%esp), %eax
    jz      L0
    shrl    $2, %ecx
    cmpl    %edx, %ecx
    cmova   %edx, %ecx
    subl    %ecx, %edx
    rep
    stosl
L0:
    movl    %edx, %ecx
    andl    $3, %edx
    shrl    $2, %ecx
    jz      L_SetDwords
    movd    %eax, %xmm0
    pshufd  $0, %xmm0, %xmm0
L1:
    movntdq %xmm0, (%edi)
    addl    $16, %edi
    loop    L1
    mfence
L_SetDwords:
    movl    %edx, %ecx
    rep
    stosl
    movl    8(%esp), %eax
    pop     %edi
    ret
