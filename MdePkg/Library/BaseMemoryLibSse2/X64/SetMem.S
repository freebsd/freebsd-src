#
# ConvertAsm.py: Automatically generated from SetMem.asm
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
#   SetMem.S
#
# Abstract:
#
#   SetMem function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemSetMem (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT8  Value
#    )
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemSetMem)
ASM_PFX(InternalMemSetMem):
    pushq   %rdi
    movq    %rcx, %rdi                  # rdi <- Buffer
    movb    %r8b, %al                   # al <- Value
    movq    %rdi, %r9                   # r9 <- Buffer as return value
    xorq    %rcx, %rcx
    subq    %rdi, %rcx
    andq    $15, %rcx                   # rcx + rdi aligns on 16-byte boundary
    jz      L0
    cmpq    %rdx, %rcx
    cmova   %rdx, %rcx
    subq    %rcx, %rdx
    rep     stosb
L0:
    movq    %rdx, %rcx
    andq    $15, %rdx
    shrq    $4, %rcx
    jz      L_SetBytes
    movb    %al, %ah                    # ax <- Value repeats twice
    movdqa  %xmm0, 0x10(%rsp)           # save xmm0
    movd    %eax, %xmm0                 # xmm0[0..16] <- Value repeats twice
    pshuflw $0, %xmm0, %xmm0            # xmm0[0..63] <- Value repeats 8 times
    movlhps %xmm0, %xmm0                # xmm0 <- Value repeats 16 times
L1:
    movntdq %xmm0, (%rdi)               # rdi should be 16-byte aligned
    add     $16, %rdi
    loop    L1
    mfence
    movdqa  0x10(%rsp), %xmm0           # restore xmm0
L_SetBytes:
    movl    %edx, %ecx                  # high 32 bits of rcx are always zero
    rep     stosb
    movq    %r9, %rax                   # rax <- Return value
    popq    %rdi
    ret
