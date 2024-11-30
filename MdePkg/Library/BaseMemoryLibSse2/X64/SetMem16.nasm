;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   SetMem16.nasm
;
; Abstract:
;
;   SetMem16 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem16 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT16 Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem16)
ASM_PFX(InternalMemSetMem16):
    push    rdi
    mov     rdi, rcx
    mov     r9, rdi
    xor     rcx, rcx
    sub     rcx, rdi
    and     rcx, 63
    mov     rax, r8
    jz      .0
    shr     rcx, 1
    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx
    rep     stosw
.0:
    mov     rcx, rdx
    and     edx, 31
    shr     rcx, 5
    jz      @SetWords
    movd    xmm0, eax
    pshuflw xmm0, xmm0, 0
    movlhps xmm0, xmm0
.1:
    movntdq [rdi], xmm0
    movntdq [rdi + 16], xmm0
    movntdq [rdi + 32], xmm0
    movntdq [rdi + 48], xmm0
    add     rdi, 64
    loop    .1
    mfence
@SetWords:
    mov     ecx, edx
    rep     stosw
    mov     rax, r9
    pop     rdi
    ret

