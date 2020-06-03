;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadMm0.Asm
;
; Abstract:
;
;   AsmReadMm0 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm0 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm0)
ASM_PFX(AsmReadMm0):
    push    eax
    push    eax
    movq    [esp], mm0
    pop     eax
    pop     edx
    ret

