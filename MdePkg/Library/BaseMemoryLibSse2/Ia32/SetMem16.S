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
#   SetMem16.asm
#
# Abstract:
#
#   SetMem16 function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemSetMem16)

#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemSetMem16 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT16 Value
#    )
#------------------------------------------------------------------------------
ASM_PFX(InternalMemSetMem16):
    push    %edi
    movl    12(%esp), %edx
    movl    8(%esp), %edi
    xorl    %ecx, %ecx
    subl    %edi, %ecx
    andl    $15, %ecx                   # ecx + edi aligns on 16-byte boundary
    movl    16(%esp), %eax
    jz      L0
    shrl    %ecx
    cmpl    %edx, %ecx
    cmova   %edx, %ecx
    subl    %ecx, %edx
    rep
    stosw
L0:
    movl    %edx, %ecx
    andl    $7, %edx
    shrl    $3, %ecx
    jz      L_SetWords
    movd    %eax, %xmm0
    pshuflw $0, %xmm0, %xmm0
    movlhps %xmm0, %xmm0
L1:
    movntdq %xmm0, (%edi)
    addl    $16, %edi
    loop    L1
    mfence
L_SetWords:
    movl    %edx, %ecx
    rep
    stosw
    movl    8(%esp), %eax
    pop     %edi
    ret
