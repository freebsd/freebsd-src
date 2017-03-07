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
#  InternalMemSetMem16 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT16 Value
#    )
#------------------------------------------------------------------------------
ASM_PFX(InternalMemSetMem16):
    push    %edi
    movl    16(%esp), %eax
    shrdl   $16, %eax, %edx
    shldl   $16, %edx, %eax
    movl    12(%esp), %edx
    movl    8(%esp), %edi
    movl    %edx, %ecx
    andl    $3, %edx
    shrl    $2, %ecx
    jz      L1
    movd    %eax, %mm0
    movd    %eax, %mm1
    psllq   $32, %mm0
    por     %mm1, %mm0
L0:
    movq    %mm0, (%edi)
    addl    $8, %edi
    loop    L0
L1:
    movl    %edx, %ecx
    rep
    stosw
    movl    8(%esp), %eax
    pop     %edi
    ret
