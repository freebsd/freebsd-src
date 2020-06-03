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

    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem64 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT64 Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem64)
ASM_PFX(InternalMemSetMem64):
    mov     eax, [esp + 4]              ; eax <- Buffer
    mov     ecx, [esp + 8]              ; ecx <- Count
    test    al, 8
    mov     edx, eax
    movq    xmm0, qword [esp + 12]
    jz      .0
    movq    qword [edx], xmm0
    add     edx, 8
    dec     ecx
.0:
    shr     ecx, 1
    jz      @SetQwords
    movlhps xmm0, xmm0
.1:
    movntdq [edx], xmm0
    lea     edx, [edx + 16]
    loop    .1
    mfence
@SetQwords:
    jnc     .2
    movq    qword [edx], xmm0
.2:
    ret

