;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadMm5.Asm
;
; Abstract:
;
;   AsmReadMm5 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm5 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm5)
ASM_PFX(AsmReadMm5):
    movq    rax, mm5
    ret

