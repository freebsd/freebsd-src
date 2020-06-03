;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   SetMem.nasm
;
; Abstract:
;
;   SetMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT8  Value
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem)
ASM_PFX(InternalMemSetMem):
    push    edi
    mov     edx, [esp + 12]             ; edx <- Count
    mov     edi, [esp + 8]              ; edi <- Buffer
    mov     al, [esp + 16]              ; al <- Value
    xor     ecx, ecx
    sub     ecx, edi
    and     ecx, 15                     ; ecx + edi aligns on 16-byte boundary
    jz      .0
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx
    rep     stosb
.0:
    mov     ecx, edx
    and     edx, 15
    shr     ecx, 4                      ; ecx <- # of DQwords to set
    jz      @SetBytes
    mov     ah, al                      ; ax <- Value | (Value << 8)
    add     esp, -16
    movdqu  [esp], xmm0                 ; save xmm0
    movd    xmm0, eax
    pshuflw xmm0, xmm0, 0               ; xmm0[0..63] <- Value repeats 8 times
    movlhps xmm0, xmm0                  ; xmm0 <- Value repeats 16 times
.1:
    movntdq [edi], xmm0                 ; edi should be 16-byte aligned
    add     edi, 16
    loop    .1
    mfence
    movdqu  xmm0, [esp]                 ; restore xmm0
    add     esp, 16                     ; stack cleanup
@SetBytes:
    mov     ecx, edx
    rep     stosb
    mov     eax, [esp + 8]              ; eax <- Buffer as return value
    pop     edi
    ret

