;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ARShiftU64.nasm
;
; Abstract:
;
;   64-bit arithmetic right shift function for IA-32
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathARShiftU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathARShiftU64)
ASM_PFX(InternalMathARShiftU64):
    mov     cl, [esp + 12]
    mov     eax, [esp + 8]
    cdq
    test    cl, 32
    jnz     .0
    mov     edx, eax
    mov     eax, [esp + 4]
.0:
    shrd    eax, edx, cl
    sar     edx, cl
    ret

