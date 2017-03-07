#
# ConvertAsm.py: Automatically generated from CopyMem.asm
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
#   CopyMem.S
#
# Abstract:
#
#   CopyMem function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemCopyMem (
#    IN VOID   *Destination,
#    IN VOID   *Source,
#    IN UINTN  Count
#    )
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemCopyMem)
ASM_PFX(InternalMemCopyMem):
    pushq   %rsi
    pushq   %rdi
    movq    %rdx, %rsi                  # rsi <- Source
    movq    %rcx, %rdi                  # rdi <- Destination
    leaq    -1(%rsi, %r8,), %r9         # r9 <- Last byte of Source
    cmpq    %rdi, %rsi
    movq    %rdi, %rax                  # rax <- Destination as return value
    jae     L0                          # Copy forward if Source > Destination
    cmpq    %rdi, %r9                   # Overlapped?
    jae     L_CopyBackward              # Copy backward if overlapped
L0:            
    xorq    %rcx, %rcx                    
    subq    %rdi, %rcx                  # rcx <- -rdi
    andq    $15, %rcx                   # rcx + rsi should be 16 bytes aligned
    jz      L1                          # skip if rcx == 0
    cmpq    %r8, %rcx
    cmova   %r8, %rcx
    subq    %rcx, %r8
    rep     movsb
L1:
    movq    %r8, %rcx
    andq    $15, %r8
    shrq    $4, %rcx                    # rcx <- # of DQwords to copy
    jz      L_CopyBytes
    movdqa  %xmm0, 0x18(%rsp)           # save xmm0 on stack
L2:
    movdqu  (%rsi), %xmm0               # rsi may not be 16-byte aligned
    movntdq  %xmm0, (%rdi)              # rdi should be 16-byte aligned
    addq    $16, %rsi
    addq    $16, %rdi
    loop    L2
    mfence
    movdqa  0x18(%rsp), %xmm0           # restore xmm0
    jmp     L_CopyBytes                 # copy remaining bytes
L_CopyBackward:
    movq    %r9, %rsi                   # rsi <- Last byte of Source
    leaq    -1(%rdi, %r8,), %rdi        # rdi <- Last byte of Destination
    std
L_CopyBytes:
    movq    %r8, %rcx
    rep     movsb
    cld
    popq    %rdi
    popq    %rsi
    ret
