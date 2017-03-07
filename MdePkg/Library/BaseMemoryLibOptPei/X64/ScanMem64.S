#
# ConvertAsm.py: Automatically generated from ScanMem64.asm
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
#   ScanMem64.S
#
# Abstract:
#
#   ScanMem64 function
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
# CONST VOID *
# EFIAPI
# InternalMemScanMem64 (
#   IN      CONST VOID                *Buffer,
#   IN      UINTN                     Length,
#   IN      UINT64                    Value
#   );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemScanMem64)
ASM_PFX(InternalMemScanMem64):
    pushq   %rdi
    movq    %rcx, %rdi
    movq    %rdx, %rcx
    movq    %r8, %rax
    repne   scasq
    leaq    -8(%rdi), %rax
    cmovnz  %rcx, %rax                    # set rax to 0 if not found
    popq    %rdi
    ret

