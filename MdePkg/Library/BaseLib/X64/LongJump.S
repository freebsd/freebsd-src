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
#   LongJump.S
#
# Abstract:
#
#   Implementation of _LongJump() on x64.
#
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# InternalLongJump (
#   IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
#   IN      UINTN                     Value
#   );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalLongJump)
ASM_PFX(InternalLongJump):
    mov     (%rcx), %rbx
    mov     0x8(%rcx), %rsp
    mov     0x10(%rcx), %rbp
    mov     0x18(%rcx), %rdi
    mov     0x20(%rcx), %rsi
    mov     0x28(%rcx), %r12
    mov     0x30(%rcx), %r13
    mov     0x38(%rcx), %r14
    mov     0x40(%rcx), %r15
    # load non-volatile fp registers
    ldmxcsr 0x50(%rcx)
    movdqu  0x58(%rcx), %xmm6
    movdqu  0x68(%rcx), %xmm7
    movdqu  0x78(%rcx), %xmm8
    movdqu  0x88(%rcx), %xmm9
    movdqu  0x98(%rcx), %xmm10
    movdqu  0xA8(%rcx), %xmm11
    movdqu  0xB8(%rcx), %xmm12
    movdqu  0xC8(%rcx), %xmm13
    movdqu  0xD8(%rcx), %xmm14
    movdqu  0xE8(%rcx), %xmm15  
    mov     %rdx, %rax          # set return value
    jmp     *0x48(%rcx)
