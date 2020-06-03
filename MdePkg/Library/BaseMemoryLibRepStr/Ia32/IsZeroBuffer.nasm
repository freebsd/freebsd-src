;------------------------------------------------------------------------------
;
; Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   IsZeroBuffer.nasm
;
; Abstract:
;
;   IsZeroBuffer function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
;  BOOLEAN
;  EFIAPI
;  InternalMemIsZeroBuffer (
;    IN CONST VOID  *Buffer,
;    IN UINTN       Length
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemIsZeroBuffer)
ASM_PFX(InternalMemIsZeroBuffer):
    push    edi
    mov     edi, [esp + 8]             ; edi <- Buffer
    mov     ecx, [esp + 12]            ; ecx <- Length
    mov     edx, ecx                   ; edx <- ecx
    shr     ecx, 2                     ; ecx <- number of dwords
    and     edx, 3                     ; edx <- number of trailing bytes
    xor     eax, eax                   ; eax <- 0, also set ZF
    repe    scasd
    jnz     @ReturnFalse               ; ZF=0 means non-zero element found
    mov     ecx, edx
    repe    scasb
    jnz     @ReturnFalse
    pop     edi
    mov     eax, 1                     ; return TRUE
    ret
@ReturnFalse:
    pop     edi
    xor     eax, eax
    ret                                ; return FALSE

