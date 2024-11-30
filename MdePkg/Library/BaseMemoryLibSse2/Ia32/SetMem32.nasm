;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   SetMem32.nasm
;
; Abstract:
;
;   SetMem32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem32 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT32 Value
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem32)
ASM_PFX(InternalMemSetMem32):
    push    edi
    mov     edx, [esp + 12]
    mov     edi, [esp + 8]
    xor     ecx, ecx
    sub     ecx, edi
    and     ecx, 15                     ; ecx + edi aligns on 16-byte boundary
    mov     eax, [esp + 16]
    jz      .0
    shr     ecx, 2
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx
    rep     stosd
.0:
    mov     ecx, edx
    and     edx, 15
    shr     ecx, 4
    jz      @SetDwords
    movd    xmm0, eax
    pshufd  xmm0, xmm0, 0
.1:
    movntdq [edi], xmm0
    movntdq [edi + 16], xmm0
    movntdq [edi + 32], xmm0
    movntdq [edi + 48], xmm0
    add     edi, 64
    loop    .1
    mfence
@SetDwords:
    mov     ecx, edx
    rep     stosd
    mov     eax, [esp + 8]
    pop     edi
    ret

