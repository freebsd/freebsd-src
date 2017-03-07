#
# ConvertAsm.py: Automatically generated from ZeroMem.asm
#
#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
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
#   ZeroMem.S
#
# Abstract:
#
#   ZeroMem function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
#  VOID *
#  InternalMemZeroMem (
#    IN VOID   *Buffer,
#    IN UINTN  Count
#    );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemZeroMem)
ASM_PFX(InternalMemZeroMem):
    pushq   %rdi
    movq    %rcx, %rdi
    movq    %rdx, %rcx
    movq    %rdi, %r8
    andq    $7, %rdx
    shrq    $3, %rcx
    jz      L_ZeroBytes
    pxor    %mm0, %mm0
L0:
    movntq  %mm0, (%rdi)
    addq    $8, %rdi
    loop    L0
    mfence
L_ZeroBytes:
    xorl    %eax, %eax
    movl    %edx, %ecx
    rep     stosb
    movq    %r8, %rax
    popq    %rdi
    ret

