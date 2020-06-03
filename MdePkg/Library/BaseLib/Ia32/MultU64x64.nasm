;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   MultU64x64.nasm
;
; Abstract:
;
;   Calculate the product of a 64-bit integer and another 64-bit integer
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathMultU64x64 (
;   IN      UINT64                    Multiplicand,
;   IN      UINT64                    Multiplier
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathMultU64x64)
ASM_PFX(InternalMathMultU64x64):
    push    ebx
    mov     ebx, [esp + 8]              ; ebx <- M1[0..31]
    mov     edx, [esp + 16]             ; edx <- M2[0..31]
    mov     ecx, ebx
    mov     eax, edx
    imul    ebx, [esp + 20]             ; ebx <- M1[0..31] * M2[32..63]
    imul    edx, [esp + 12]             ; edx <- M1[32..63] * M2[0..31]
    add     ebx, edx                    ; carries are abandoned
    mul     ecx                         ; edx:eax <- M1[0..31] * M2[0..31]
    add     edx, ebx                    ; carries are abandoned
    pop     ebx
    ret

