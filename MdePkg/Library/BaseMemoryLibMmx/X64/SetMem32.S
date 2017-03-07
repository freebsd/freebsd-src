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
    movd    %r8, %mm0                   # mm0 <- Value
    movq    %rcx, %rax                  # rax <- Buffer
    xchgq   %rdx, %rcx                  # rcx <- Count  rdx <- Buffer
    shrq    $1, % rcx                   # rcx <- # of qwords to set
    jz      L_SetDwords
   .byte    0x0f, 0x70, 0x0C0, 0x44
L0:
    movntq  %mm0, (%rdx) 
    leaq    8(%rdx), %rdx               # use "lea" to avoid flag changes
    loop    L0
    mfence
L_SetDwords:
    jnc     L1
    movd    %mm0, (%rdx) 
L1:
    ret

