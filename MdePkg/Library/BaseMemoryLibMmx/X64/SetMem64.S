#
# ConvertAsm.py: Automatically generated from SetMem64.asm
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
#   SetMem64.S
#
# Abstract:
#
#   SetMem64 function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
#  VOID *
#  InternalMemSetMem64 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT64 Value
#    )
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemSetMem64)
ASM_PFX(InternalMemSetMem64):
    movd      %r8, %mm0                 #Value
    movq      %rcx, %rax                #rax <- Buffer
    xchg      %rdx, %rcx                #rcx <- Count
L0:
    movntq  %mm0, (%rdx) 
    addq    $8, %rdx
    loop    L0
    mfence
    ret

