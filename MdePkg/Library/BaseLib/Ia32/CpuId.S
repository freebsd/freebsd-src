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

ASM_GLOBAL  ASM_PFX(AsmCpuid)

#------------------------------------------------------------------------------
#  VOID
#  EFIAPI
#  AsmCpuid (
#    IN   UINT32  RegisterInEax,
#    OUT  UINT32  *RegisterOutEax  OPTIONAL,
#    OUT  UINT32  *RegisterOutEbx  OPTIONAL,
#    OUT  UINT32  *RegisterOutEcx  OPTIONAL,
#    OUT  UINT32  *RegisterOutEdx  OPTIONAL
#    )
#------------------------------------------------------------------------------
ASM_PFX(AsmCpuid):
    push    %ebx
    push    %ebp
    movl    %esp, %ebp
    movl    12(%ebp), %eax
    cpuid
    push    %ecx
    movl    16(%ebp), %ecx
    jecxz   L1
    movl    %eax, (%ecx)
L1:
    movl    20(%ebp), %ecx
    jecxz   L2
    movl    %ebx, (%ecx)
L2:
    movl    24(%ebp), %ecx
    jecxz   L3
    popl    (%ecx)
L3:
    movl    28(%ebp), %ecx
    jecxz   L4
    movl    %edx, (%ecx)
L4:
    movl    12(%ebp), %eax
    leave
    pop     %ebx
    ret
