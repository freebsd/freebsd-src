;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   SetJump.Asm
;
; Abstract:
;
;   Implementation of SetJump() on x64.
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

extern ASM_PFX(InternalAssertJumpBuffer)

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; SetJump (
;   OUT     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
;   );
;------------------------------------------------------------------------------
global ASM_PFX(SetJump)
ASM_PFX(SetJump):
    push    rcx
    add     rsp, -0x20
    call    ASM_PFX(InternalAssertJumpBuffer)
    add     rsp, 0x20
    pop     rcx
    pop     rdx
    mov     [rcx], rbx
    mov     [rcx + 8], rsp
    mov     [rcx + 0x10], rbp
    mov     [rcx + 0x18], rdi
    mov     [rcx + 0x20], rsi
    mov     [rcx + 0x28], r12
    mov     [rcx + 0x30], r13
    mov     [rcx + 0x38], r14
    mov     [rcx + 0x40], r15
    mov     [rcx + 0x48], rdx
    ; save non-volatile fp registers
    stmxcsr [rcx + 0x50]
    movdqu  [rcx + 0x58], xmm6
    movdqu  [rcx + 0x68], xmm7
    movdqu  [rcx + 0x78], xmm8
    movdqu  [rcx + 0x88], xmm9
    movdqu  [rcx + 0x98], xmm10
    movdqu  [rcx + 0xA8], xmm11
    movdqu  [rcx + 0xB8], xmm12
    movdqu  [rcx + 0xC8], xmm13
    movdqu  [rcx + 0xD8], xmm14
    movdqu  [rcx + 0xE8], xmm15
    xor     rax, rax
    jmp     rdx

