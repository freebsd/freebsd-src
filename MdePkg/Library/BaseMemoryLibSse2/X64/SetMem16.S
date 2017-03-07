#
# ConvertAsm.py: Automatically generated from SetMem16.asm
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
#   SetMem16.S
#
# Abstract:
#
#   SetMem16 function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemSetMem16 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT16 Value
#    )
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemSetMem16)
ASM_PFX(InternalMemSetMem16):
    pushq   %rdi
    movq    %rcx, %rdi
    movq    %rdi, %r9
    xorq    %rcx, %rcx
    subq    %rdi, %rcx
    andq    $15, %rcx
    movq    %r8, %rax
    jz      L0
    shrq    $1, %rcx
    cmpq    %rdx, %rcx
    cmova   %rdx, %rcx
    subq    %rcx, %rdx
    rep     stosw
L0:
    movq    %rdx, %rcx
    andl    $7, %edx
    shrq    $3, %rcx
    jz      L_SetWords
    movd    %eax, %xmm0
    pshuflw $0, %xmm0, %xmm0
    movlhps %xmm0, %xmm0
L1:
    movntdq %xmm0, (%rdi)
    addq    $16, %rdi
    loop    L1
    mfence
L_SetWords:
    movl    %edx, %ecx
    rep     stosw
    movq    %r9, %rax
    popq    %rdi
    ret
