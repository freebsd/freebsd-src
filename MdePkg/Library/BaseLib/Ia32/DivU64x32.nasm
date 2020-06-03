;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   DivU64x32.nasm
;
; Abstract:
;
;   Calculate the quotient of a 64-bit integer by a 32-bit integer
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathDivU64x32 (
;   IN      UINT64                    Dividend,
;   IN      UINT32                    Divisor
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathDivU64x32)
ASM_PFX(InternalMathDivU64x32):
    mov     eax, [esp + 8]
    mov     ecx, [esp + 12]
    xor     edx, edx
    div     ecx
    push    eax                     ; save quotient on stack
    mov     eax, [esp + 8]
    div     ecx
    pop     edx                     ; restore high-order dword of the quotient
    ret

