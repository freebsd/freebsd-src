;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   SetMem64.nasm
;
; Abstract:
;
;   SetMem64 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem64 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT64 Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem64)
ASM_PFX(InternalMemSetMem64):
    mov     rax, rcx                    ; rax <- Buffer
    xchg    rcx, rdx                    ; rcx <- Count & rdx <- Buffer
    test    dl, 8
    movq    xmm0, r8
    jz      .0
    mov     [rdx], r8
    add     rdx, 8
    dec     rcx
.0:
    shr     rcx, 1
    jz      @SetQwords
    movlhps xmm0, xmm0
.1:
    movntdq [rdx], xmm0
    lea     rdx, [rdx + 16]
    loop    .1
    mfence
@SetQwords:
    jnc     .2
    mov     [rdx], r8
.2:
    ret

