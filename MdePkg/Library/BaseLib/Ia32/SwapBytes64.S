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
#   CpuId.S
#
# Abstract:
#
#   AsmCpuid function
#
# Notes:
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
# UINT64
# EFIAPI
# InternalMathSwapBytes64 (
#   IN      UINT64                    Operand
#   );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMathSwapBytes64)
ASM_PFX(InternalMathSwapBytes64):
    movl    8(%esp), %eax               # eax <- upper 32 bits
    movl    4(%esp), %edx               # edx <- lower 32 bits
    bswapl  %eax
    bswapl  %edx
    ret
