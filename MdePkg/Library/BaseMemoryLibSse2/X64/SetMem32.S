#
# ConvertAsm.py: Automatically generated from SetMem32.asm
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
#   SetMem32.S
#
# Abstract:
#
#   SetMem32 function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemSetMem32 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT32 Value
#    );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemSetMem32)
ASM_PFX(InternalMemSetMem32):
    pushq   %rdi
    movq    %rcx, %rdi
    movq    %rdi, %r9
    xorq    %rcx, %rcx
    subq    %rdi, %rcx
    andq    $15, %rcx
    movq    %r8, %rax
    jz      L0
    shrq    $2, %rcx
    cmpq    %rdx, %rcx
    cmova   %rdx, %rcx
    subq    %rcx, %rdx
    rep     stosl
L0:
    movq    %rdx, %rcx
    andl    $3, %edx
    shrq    $2, %rcx
    jz      L_SetDwords
    movd    %eax, %xmm0
    pshufd  $0, %xmm0, %xmm0
L1:
    movntdq %xmm0, (%rdi)
    addq    $16, %rdi
    loop    L1
    mfence
L_SetDwords:
    movl    %edx, %ecx
    rep     stosl
    movq    %r9, %rax
    popq    %rdi
    ret
