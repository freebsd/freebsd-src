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
#   SetMem.S
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
    movl    12(%esp),%ecx
    movb    16(%esp),%al
    movb    %al, %ah
    shrd    $16, %eax, %edx
    shld    $16, %edx, %eax
    movl    %ecx, %edx
    movl    8(%esp),%edi
    shr    $2, %ecx
    rep stosl
    movl    %edx, %ecx
    andl    $3, %ecx
    rep stosb
    movl    8(%esp),%eax
    pop     %edi
    ret
