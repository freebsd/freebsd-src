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
    push    %rdi
    movq    %r8, %rax
    movb    %al, %ah
    movd    %rax, %mm0
    movq    %rcx, %r8
    movq    %r8, %rdi                     # rdi <- Buffer
    movq    %rdx, %rcx
    andq    $7, %rdx 
    shrq    $3, %rcx
    jz      L_SetBytes
    .byte   0x0f, 0x70, 0x0C0, 0x00
L0:
    movntq  %mm0, (%rdi) 
    addq    $8, %rdi
    loop    L0
    mfence
L_SetBytes:
    movl    %edx, %ecx
    rep       stosb
    movq    %r8, %rax
    pop     %rdi
    ret

