;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadMm6.Asm
;
; Abstract:
;
;   AsmReadMm6 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm6 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm6)
ASM_PFX(AsmReadMm6):
    movq    rax, mm6
    ret

