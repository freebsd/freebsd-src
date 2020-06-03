;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   LongJump.Asm
;
; Abstract:
;
;   Implementation of _LongJump() on x64.
;
;------------------------------------------------------------------------------

%include "Nasm.inc"

    DEFAULT REL
    SECTION .text

extern ASM_PFX(PcdGet32 (PcdControlFlowEnforcementPropertyMask))

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalLongJump (
;   IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
;   IN      UINTN                     Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalLongJump)
ASM_PFX(InternalLongJump):

    mov     eax, [ASM_PFX(PcdGet32 (PcdControlFlowEnforcementPropertyMask))]
    test    eax, eax
    jz      CetDone
    mov     rax, cr4
    bt      eax, 23                      ; check if CET is enabled
    jnc     CetDone

    push    rdx                          ; save rdx

    mov     rdx, [rcx + 0xF8]            ; rdx = target SSP
    READSSP_RAX
    sub     rdx, rax                     ; rdx = delta
    mov     rax, rdx                     ; rax = delta

    shr     rax, 3                       ; rax = delta/sizeof(UINT64)
    INCSSP_RAX

    pop     rdx                          ; restore rdx
CetDone:

    mov     rbx, [rcx]
    mov     rsp, [rcx + 8]
    mov     rbp, [rcx + 0x10]
    mov     rdi, [rcx + 0x18]
    mov     rsi, [rcx + 0x20]
    mov     r12, [rcx + 0x28]
    mov     r13, [rcx + 0x30]
    mov     r14, [rcx + 0x38]
    mov     r15, [rcx + 0x40]
    ; load non-volatile fp registers
    ldmxcsr [rcx + 0x50]
    movdqu  xmm6,  [rcx + 0x58]
    movdqu  xmm7,  [rcx + 0x68]
    movdqu  xmm8,  [rcx + 0x78]
    movdqu  xmm9,  [rcx + 0x88]
    movdqu  xmm10, [rcx + 0x98]
    movdqu  xmm11, [rcx + 0xA8]
    movdqu  xmm12, [rcx + 0xB8]
    movdqu  xmm13, [rcx + 0xC8]
    movdqu  xmm14, [rcx + 0xD8]
    movdqu  xmm15, [rcx + 0xE8]
    mov     rax, rdx               ; set return value
    jmp     qword [rcx + 0x48]

