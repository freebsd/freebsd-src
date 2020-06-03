;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadMm1.Asm
;
; Abstract:
;
;   AsmReadMm1 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm1 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm1)
ASM_PFX(AsmReadMm1):
    push    eax
    push    eax
    movq    [esp], mm1
    pop     eax
    pop     edx
    ret

