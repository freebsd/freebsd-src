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
    movq    %r8, %rax      # rax = Value
    movq    %rcx, %rdi     # rdi = Buffer
    xchgq   %rdx, %rcx     # rcx = Count, rdx = Buffer
    rep     stosb
    movq    %rdx, %rax     # rax = Buffer
    popq    %rdi
    ret

