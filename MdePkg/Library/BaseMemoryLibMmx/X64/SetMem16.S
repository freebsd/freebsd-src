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
    pushq    %rdi
    movq     %r8, %rax 
    movd     %rax, %mm0 
    movq     %rcx, %r8 
    movq     %r8, %rdi 
    movq     %rdx, %rcx 
    andl     $3, %edx 
    shrq     $2, %rcx
    jz       L_SetWords
    .byte    0x0f, 0x70, 0x0C0, 0x00
L0:
    movntq   %mm0, (%rdi) 
    addq     $8, %rdi 
    loop     L0
    mfence
L_SetWords:
    movl    %edx, %ecx 
    rep     stosw
    movq    %r8, %rax
    popq    %rdi
    ret

