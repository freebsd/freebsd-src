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
#   CompareMem.Asm
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

ASM_GLOBAL ASM_PFX(InternalMemCompareMem)

#------------------------------------------------------------------------------
# INTN
# EFIAPI
# InternalMemCompareMem (
#   IN      CONST VOID                *DestinationBuffer,
#   IN      CONST VOID                *SourceBuffer,
#   IN      UINTN                     Length
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemCompareMem):
    push    %esi
    push    %edi
    movl    12(%esp), %esi
    movl    16(%esp), %edi
    movl    20(%esp), %ecx
    repe    cmpsb
    movzbl  -1(%esi), %eax
    movzbl  -1(%edi), %edx
    subl    %edx, %eax
    pop     %edi
    pop     %esi
    ret
