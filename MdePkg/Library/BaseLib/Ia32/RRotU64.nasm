;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   RRotU64.nasm
;
; Abstract:
;
;   64-bit right rotation for Ia32
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathRRotU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathRRotU64)
ASM_PFX(InternalMathRRotU64):
    push    ebx
    mov     cl, [esp + 16]
    mov     eax, [esp + 8]
    mov     edx, [esp + 12]
    shrd    ebx, eax, cl
    shrd    eax, edx, cl
    rol     ebx, cl
    shrd    edx, ebx, cl
    test    cl, 32                      ; Count >= 32?
    jz      .0
    mov     ecx, eax                    ; switch eax & edx if Count >= 32
    mov     eax, edx
    mov     edx, ecx
.0:
    pop     ebx
    ret

