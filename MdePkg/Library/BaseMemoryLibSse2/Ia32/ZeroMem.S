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
#   ZeroMem.asm
#
# Abstract:
#
#   ZeroMem function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemZeroMem)

#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemZeroMem (
#    IN VOID   *Buffer,
#    IN UINTN  Count
#    );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemZeroMem):
    push    %edi
    movl    8(%esp), %edi
    movl    12(%esp), %edx
    xorl    %ecx, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    andl    $15, %ecx
    jz      L0
    cmpl    %edx, %ecx
    cmova   %edx, %ecx
    subl    %ecx, %edx
    rep
    stosb
L0:
    movl    %edx, %ecx
    andl    $15, %edx
    shrl    $4, %ecx
    jz      L_ZeroBytes
    pxor    %xmm0, %xmm0
L1:
    movntdq %xmm0, (%edi)
    addl    $16, %edi
    loop    L1
    mfence
L_ZeroBytes:
    movl    %edx, %ecx
    rep
    stosb
    movl    8(%esp), %eax
    pop     %edi
    ret
