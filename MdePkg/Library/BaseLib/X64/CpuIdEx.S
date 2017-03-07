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
#   CpuIdEx.S
#
# Abstract:
#
#   AsmCpuidEx function
#
# Notes:
#
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
#  UINT32
#  EFIAPI
#  AsmCpuidEx (
#    IN   UINT32  RegisterInEax,
#    IN   UINT32  RegisterInEcx,
#    OUT  UINT32  *RegisterOutEax  OPTIONAL,
#    OUT  UINT32  *RegisterOutEbx  OPTIONAL,
#    OUT  UINT32  *RegisterOutEcx  OPTIONAL,
#    OUT  UINT32  *RegisterOutEdx  OPTIONAL
#    )
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(AsmCpuidEx)
ASM_PFX(AsmCpuidEx):
    push    %rbx
    movl    %ecx,%eax
    movl    %edx,%ecx
    push    %rax                  # save Index on stack
    cpuid
    mov     0x38(%rsp), %r10
    test    %r10, %r10
    jz      L1
    mov     %ecx,(%r10)
L1: 
    mov     %r8, %rcx
    jrcxz   L2
    movl    %eax,(%rcx)
L2: 
    mov     %r9, %rcx
    jrcxz   L3
    mov     %ebx, (%rcx)
L3: 
    mov     0x40(%rsp), %rcx
    jrcxz   L4
    mov     %edx, (%rcx)
L4: 
    pop     %rax                  # restore Index to rax as return value
    pop     %rbx
    ret
