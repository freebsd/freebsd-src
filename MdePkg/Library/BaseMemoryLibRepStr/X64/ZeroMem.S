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
    pushq   %rcx
    xorq    %rax, %rax
    movq    %rcx, %rdi
    movq    %rdx, %rcx
    shrq    $3, %rcx
    andq    $7, %rdx
    rep     stosq
    movl    %edx, %ecx
    rep     stosb
    popq    %rax
    popq    %rdi
    ret

