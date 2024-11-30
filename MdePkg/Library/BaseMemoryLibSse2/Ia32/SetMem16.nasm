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

    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem16 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT16 Value
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem16)
ASM_PFX(InternalMemSetMem16):
    push    edi
    mov     edx, [esp + 12]
    mov     edi, [esp + 8]
    xor     ecx, ecx
    sub     ecx, edi
    and     ecx, 63                     ; ecx + edi aligns on 16-byte boundary
    mov     eax, [esp + 16]
    jz      .0
    shr     ecx, 1
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx
    rep     stosw
.0:
    mov     ecx, edx
    and     edx, 31
    shr     ecx, 5
    jz      @SetWords
    movd    xmm0, eax
    pshuflw xmm0, xmm0, 0
    movlhps xmm0, xmm0
.1:
    movntdq [edi], xmm0                 ; edi should be 16-byte aligned
    movntdq [edi + 16], xmm0
    movntdq [edi + 32], xmm0
    movntdq [edi + 48], xmm0
    add     edi, 64
    loop    .1
    mfence
@SetWords:
    mov     ecx, edx
    rep     stosw
    mov     eax, [esp + 8]
    pop     edi
    ret

