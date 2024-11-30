;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadMm4.Asm
;
; Abstract:
;
;   AsmReadMm4 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm4 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm4)
ASM_PFX(AsmReadMm4):
    movq    rax, mm4
    ret

