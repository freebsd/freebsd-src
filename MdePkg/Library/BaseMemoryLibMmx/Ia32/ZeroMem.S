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
    movl    12(%esp), %ecx
    movl    %ecx, %edx
    shrl    $3, %ecx
    jz      L_ZeroBytes
    pxor    %mm0, %mm0
L0:
    movq    %mm0, (%edi)
    addl    $8, %edi
    loop    L0
L_ZeroBytes:
    andl    $7, %edx
    xorl    %eax, %eax
    movl    %edx, %ecx
    rep
    stosb
    movl    8(%esp), %eax
    pop     %edi
    ret
