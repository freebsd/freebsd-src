;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ZeroMem.nasm
;
; Abstract:
;
;   ZeroMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemZeroMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemZeroMem)
ASM_PFX(InternalMemZeroMem):
    push    rdi
    mov     rdi, rcx
    xor     rcx, rcx
    xor     eax, eax
    sub     rcx, rdi
    and     rcx, 63
    mov     r8, rdi
    jz      .0
    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx
    rep     stosb
.0:
    mov     rcx, rdx
    and     edx, 63
    shr     rcx, 6
    jz      @ZeroBytes
    pxor    xmm0, xmm0
.1:
    movntdq [rdi], xmm0
    movntdq [rdi + 16], xmm0
    movntdq [rdi + 32], xmm0
    movntdq [rdi + 48], xmm0
    add     rdi, 64
    loop    .1
    mfence
@ZeroBytes:
    mov     ecx, edx
    rep     stosb
    mov     rax, r8
    pop     rdi
    ret

