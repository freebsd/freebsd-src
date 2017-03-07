#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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

ASM_GLOBAL ASM_PFX(InternalMemScanMem64)

#------------------------------------------------------------------------------
# CONST VOID *
# EFIAPI
# InternalMemScanMem64 (
#   IN      CONST VOID                *Buffer,
#   IN      UINTN                     Length,
#   IN      UINT64                    Value
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemScanMem64):
    push    %edi
    movl    12(%esp), %ecx
    movl    16(%esp), %eax
    movl    20(%esp), %edx
    movl    8(%esp), %edi
L0:
    cmpl    (%edi), %eax
    leal    8(%edi), %edi
    loopne  L0
    jne     L1
    cmpl    -4(%edi), %edx
    jecxz   L1
    jne     L0
L1:
    leal    -8(%edi), %eax
    cmovne  %ecx, %eax
    pop     %edi
    ret
