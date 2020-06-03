;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   DivU64x32.asm
;
; Abstract:
;
;   Calculate the remainder of a 64-bit integer by a 32-bit integer
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; InternalMathModU64x32 (
;   IN      UINT64                    Dividend,
;   IN      UINT32                    Divisor
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathModU64x32)
ASM_PFX(InternalMathModU64x32):
    mov     eax, [esp + 8]
    mov     ecx, [esp + 12]
    xor     edx, edx
    div     ecx
    mov     eax, [esp + 4]
    div     ecx
    mov     eax, edx
    ret

