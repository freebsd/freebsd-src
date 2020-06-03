;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   LShiftU64.nasm
;
; Abstract:
;
;   64-bit left shift function for IA-32
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathLShiftU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathLShiftU64)
ASM_PFX(InternalMathLShiftU64):
    mov     cl, [esp + 12]
    xor     eax, eax
    mov     edx, [esp + 4]
    test    cl, 32                      ; Count >= 32?
    jnz     .0
    mov     eax, edx
    mov     edx, [esp + 8]
.0:
    shld    edx, eax, cl
    shl     eax, cl
    ret

