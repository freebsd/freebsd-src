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
#   ScanMem8.S
#
# Abstract:
#
#   ScanMem8 function
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

ASM_GLOBAL ASM_PFX(InternalMemScanMem8)

#------------------------------------------------------------------------------
# CONST VOID *
# EFIAPI
# InternalMemScanMem8 (
#   IN      CONST VOID                *Buffer,
#   IN      UINTN                     Length,
#   IN      UINT8                     Value
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemScanMem8):
    push    %edi
    movl    12(%esp), %ecx
    movl    8(%esp), %edi
    movb    16(%esp), %al
    repne   scasb
    leal    -1(%edi), %eax
    cmovnz  %ecx, %eax
    pop     %edi
    ret
