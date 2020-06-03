;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   CpuId.Asm
;
; Abstract:
;
;   AsmCpuid function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathSwapBytes64 (
;   IN      UINT64                    Operand
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathSwapBytes64)
ASM_PFX(InternalMathSwapBytes64):
    mov     eax, [esp + 8]              ; eax <- upper 32 bits
    mov     edx, [esp + 4]              ; edx <- lower 32 bits
    bswap   eax
    bswap   edx
    ret

