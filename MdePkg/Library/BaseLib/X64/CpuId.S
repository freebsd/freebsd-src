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
ASM_GLOBAL ASM_PFX(AsmCpuid)
ASM_PFX(AsmCpuid):
    push    %rbx
    mov     %ecx, %eax
    push    %rax                         # save Index on stack
    push    %rdx
    cpuid
    test    %r9, %r9
    jz      L1
    mov     %ecx, (%r9)
L1:
    pop     %rcx
    jrcxz   L2
    mov     %eax, (%rcx)
L2:
    mov     %r8, %rcx
    jrcxz   L3
    mov     %ebx, (%rcx)
L3:
    mov     0x38(%rsp), %rcx
    jrcxz   L4
    mov     %edx, (%rcx)
L4:
    pop     %rax                         # restore Index to rax as return value
    pop     %rbx
    ret
