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
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem)
ASM_PFX(InternalMemSetMem):
    push    edi
    mov     al, [esp + 16]
    mov     ah, al
    shrd    edx, eax, 16
    shld    eax, edx, 16
    mov     ecx, [esp + 12]             ; ecx <- Count
    mov     edi, [esp + 8]              ; edi <- Buffer
    mov     edx, ecx
    and     edx, 7
    shr     ecx, 3                      ; # of Qwords to set
    jz      @SetBytes
    add     esp, -0x10
    movq    [esp], mm0                  ; save mm0
    movq    [esp + 8], mm1              ; save mm1
    movd    mm0, eax
    movd    mm1, eax
    psllq   mm0, 32
    por     mm0, mm1                    ; fill mm0 with 8 Value's
.0:
    movq    [edi], mm0
    add     edi, 8
    loop    .0
    movq    mm0, [esp]                  ; restore mm0
    movq    mm1, [esp + 8]              ; restore mm1
    add     esp, 0x10                    ; stack cleanup
@SetBytes:
    mov     ecx, edx
    rep     stosb
    mov     eax, [esp + 8]              ; eax <- Buffer as return value
    pop     edi
    ret

