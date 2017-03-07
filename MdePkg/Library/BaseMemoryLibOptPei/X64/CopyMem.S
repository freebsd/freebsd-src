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
    leaq    -1(%rsi, %r8,), %r9         # r9 <- End of Source
    cmpq    %rdi, %rsi
    movq    %rdi, %rax                  # rax <- Destination as return value
    jae     _InternalMemCopyMem_al_0000
    cmpq    %rdi, %r9
    jae     _atSym_CopyBackward         # Copy backward if overlapped
_InternalMemCopyMem_al_0000:
    movq    %r8, %rcx
    andq    $7, %r8
    shrq    $3, %rcx
    rep     movsq                       # Copy as many Qwords as possible
    jmp     _atSym_CopyBytes
_atSym_CopyBackward:
    movq    %r9, %rsi                   # rsi <- End of Source
    leaq    -1(%rdi, %r8), %rdi         # rdi <- End of Destination
    std                                 # set direction flag
_atSym_CopyBytes:
    movq    %r8, %rcx
    rep     movsb                       # Copy bytes backward
    cld
    popq    %rdi
    popq    %rsi
    ret

