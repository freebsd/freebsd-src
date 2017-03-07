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
#   SetMem.asm
#
# Abstract:
#
#   SetMem function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemSetMem)

#------------------------------------------------------------------------------
#  VOID *
#  InternalMemSetMem (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT8  Value
#    )
#------------------------------------------------------------------------------
ASM_PFX(InternalMemSetMem):
    push    %edi
    movb    16(%esp), %al
    movb    %al, %ah
    shrdl   $16, %eax, %edx
    shldl   $16, %edx, %eax
    movl    12(%esp), %ecx              # ecx <- Count
    movl    8(%esp), %edi               # edi <- Buffer
    movl    %ecx, %edx
    andl    $7, %edx
    shrl    $3, %ecx                    # # of Qwords to set
    jz      L1
    addl    $-16, %esp
    movq    %mm0, (%esp)                # save mm0
    movq    %mm1, 8(%esp)               # save mm1
    movd    %eax, %mm0
    movd    %eax, %mm1
    psllq   $32, %mm0
    por     %mm1, %mm0                  # fill mm0 with 8 Value's
L0:
    movq    %mm0, (%edi)
    addl    $8, %edi
    loop    L0
    movq    (%esp), %mm0                # restore mm0
    movq    8(%esp), %mm1               # restore mm1
    addl    $0x10, %esp                 # stack cleanup
L1:
    movl    %edx, %ecx
    rep
    stosb
    movl    8(%esp), %eax               # eax <- Buffer as return value
    pop     %edi
    ret
