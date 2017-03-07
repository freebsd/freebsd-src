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
#   SetJump.S
#
# Abstract:
#
#   Implementation of SetJump() on x86_64
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(SetJump)
ASM_PFX(SetJump):
    push   %rcx
    add    $0xffffffffffffffe0,%rsp
    call   ASM_PFX(InternalAssertJumpBuffer)
    add    $0x20,%rsp
    pop    %rcx
    pop    %rdx
    mov    %rbx,(%rcx)
    mov    %rsp,0x8(%rcx)
    mov    %rbp,0x10(%rcx)
    mov    %rdi,0x18(%rcx)
    mov    %rsi,0x20(%rcx)
    mov    %r12,0x28(%rcx)
    mov    %r13,0x30(%rcx)
    mov    %r14,0x38(%rcx)
    mov    %r15,0x40(%rcx)
    mov    %rdx,0x48(%rcx)
    # save non-volatile fp registers
    stmxcsr 0x50(%rcx)
    movdqu  %xmm6, 0x58(%rcx) 
    movdqu  %xmm7, 0x68(%rcx)
    movdqu  %xmm8, 0x78(%rcx)
    movdqu  %xmm9, 0x88(%rcx)
    movdqu  %xmm10, 0x98(%rcx)
    movdqu  %xmm11, 0xA8(%rcx)
    movdqu  %xmm12, 0xB8(%rcx)
    movdqu  %xmm13, 0xC8(%rcx)
    movdqu  %xmm14, 0xD8(%rcx)
    movdqu  %xmm15, 0xE8(%rcx)     
    xor    %rax,%rax
    jmpq   *%rdx
