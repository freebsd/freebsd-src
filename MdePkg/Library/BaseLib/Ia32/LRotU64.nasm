;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   LRotU64.nasm
;
; Abstract:
;
;   64-bit left rotation for Ia32
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathLRotU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathLRotU64)
ASM_PFX(InternalMathLRotU64):
    push    ebx
    mov     cl, [esp + 16]
    mov     edx, [esp + 12]
    mov     eax, [esp + 8]
    shld    ebx, edx, cl
    shld    edx, eax, cl
    ror     ebx, cl
    shld    eax, ebx, cl
    test    cl, 32                      ; Count >= 32?
    jz      .0
    mov     ecx, eax
    mov     eax, edx
    mov     edx, ecx
.0:
    pop     ebx
    ret

