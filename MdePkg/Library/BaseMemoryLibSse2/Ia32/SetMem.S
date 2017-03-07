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
#   SetMem.asm
#
# Abstract:
#
#   SetMem function
#
# Notes:
#
#------------------------------------------------------------------------------


    #.MODEL flat,C
    .xmm:
    .code:

#------------------------------------------------------------------------------
#  VOID *
#  _mem_SetMem (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT8  Value
#    );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemSetMem)
ASM_PFX(InternalMemSetMem):
    push    %edi
    movl    12(%esp), %edx              # edx <- Count
    movl    8(%esp), %edi               # edi <- Buffer
    movb    16(%esp), %al               # al <- Value
    xorl    %ecx, %ecx
    subl    %edi, %ecx
    andl    $15, %ecx                   # ecx + edi aligns on 16-byte boundary
    jz      L0
    cmpl    %edx, %ecx
    cmova   %edx, %ecx
    subl    %ecx, %edx
    rep
    stosb
L0:
    movl    %edx, %ecx
    andl    $15, %edx
    shrl    $4, %ecx                    # ecx <- # of DQwords to set
    jz      L_SetBytes
    movb    %al, %ah                    # ax <- Value | (Value << 8)
    addl    $-16, %esp
    movdqu  %xmm0, (%esp)
    movd    %eax, %xmm0
    pshuflw $0, %xmm0, %xmm0
    movlhps %xmm0, %xmm0
L1:
    movntdq %xmm0, (%edi)
    addl    $16, %edi
    loop   L1
    mfence
    movdqu  (%esp), %xmm0
    addl    $16, %esp                   # stack cleanup
L_SetBytes:
    movl    %edx, %ecx
    rep
    stosb
    movl    8(%esp), %eax               # eax <- Buffer as return value
    pop     %edi
    ret
