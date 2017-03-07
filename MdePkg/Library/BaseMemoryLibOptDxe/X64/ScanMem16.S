#
# ConvertAsm.py: Automatically generated from ScanMem16.asm
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
#   ScanMem16.S
#
# Abstract:
#
#   ScanMem16 function
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
# InternalMemScanMem16 (
#   IN      CONST VOID          *Buffer,
#   IN      UINTN                     Length,
#   IN      UINT16                    Value
#   );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMemScanMem16)
ASM_PFX(InternalMemScanMem16):
    pushq  %rdi
    movq   %rcx, %rdi
    movq   %r8, %rax
    movq   %rdx, %rcx
    repne   scasw
    leaq    -2(%rdi), %rax
    cmovnz  %rcx, %rax
    popq    %rdi
    ret

