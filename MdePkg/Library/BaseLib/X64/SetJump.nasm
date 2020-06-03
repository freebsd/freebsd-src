;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
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

%include "Nasm.inc"

    DEFAULT REL
    SECTION .text

extern ASM_PFX(InternalAssertJumpBuffer)
extern ASM_PFX(PcdGet32 (PcdControlFlowEnforcementPropertyMask))

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

    xor     rax, rax
    mov     [rcx + 0xF8], rax            ; save 0 to SSP

    mov     eax, [ASM_PFX(PcdGet32 (PcdControlFlowEnforcementPropertyMask))]
    test    eax, eax
    jz      CetDone
    mov     rax, cr4
    bt      eax, 23                      ; check if CET is enabled
    jnc     CetDone

    mov     rax, 1
    INCSSP_RAX                           ; to read original SSP
    READSSP_RAX
    mov     [rcx + 0xF8], rax            ; save SSP

CetDone:

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

