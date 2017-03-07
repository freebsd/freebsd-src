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
#   ZeroMem.Asm
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
#  InternalMemZeroMem (
#    IN VOID   *Buffer,
#    IN UINTN  Count
#    );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemZeroMem):
    push    %edi
    xorl    %eax,%eax
    movl    8(%esp),%edi
    movl    12(%esp),%ecx
    movl    %ecx,%edx
    shrl    $2,%ecx
    andl    $3,%edx
    pushl   %edi
    rep
    stosl
    movl    %edx,%ecx
    rep
    stosb
    popl    %eax
    pop     %edi
    ret
