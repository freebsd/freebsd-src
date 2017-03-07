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
    xorq    %rcx, %rcx
    xorl    %eax, %eax
    subq    %rdi, %rcx
    andq    $15, %rcx
    movq    %rdi, %r8
    jz      L0
    cmpq    %rdx, %rcx
    cmova   %rdx, %rcx
    subq    %rcx, %rdx
    rep     stosb
L0:
    movq    %rdx, %rcx
    andl    $15, %edx
    shrq    $4, %rcx
    jz      L_ZeroBytes
    pxor    %xmm0, %xmm0
L1:
    movntdq %xmm0, (%rdi)                 # rdi should be 16-byte aligned
    addq    $16, %rdi
    loop    L1
    mfence
L_ZeroBytes:
    movl    %edx, %ecx
    rep     stosb
    movq    %r8, %rax
    popq    %rdi
    ret
