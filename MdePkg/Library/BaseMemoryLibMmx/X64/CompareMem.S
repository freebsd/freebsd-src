#
# ConvertAsm.py: Automatically generated from CompareMem.asm
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
#   CompareMem.S
#
# Abstract:
#
#   CompareMem function
#
# Notes:
#
#   The following BaseMemoryLib instances contain the same copy of this file:
#
#       BaseMemoryLibRepStr
#       BaseMemoryLibMmx
#       BaseMemoryLibSse2
#       BaseMemoryLibOptDxe
#       BaseMemoryLibOptPei
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
# INTN
# EFIAPI
# InternalMemCompareMem (
#   IN      CONST VOID                *DestinationBuffer,
#   IN      CONST VOID                *SourceBuffer,
#   IN      UINTN                     Length
#   );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemCompareMem)
ASM_PFX(InternalMemCompareMem):
    pushq   %rsi
    pushq   %rdi
    movq    %rcx, %rsi 
    movq    %rdx, %rdi 
    movq    %r8, %rcx 
    repe    cmpsb
    movzbq  -1(%rsi), %rax
    movzbq  -1(%rdi), %rdx
    subq    %rdx, %rax 
    popq    %rdi
    popq    %rsi
    ret

