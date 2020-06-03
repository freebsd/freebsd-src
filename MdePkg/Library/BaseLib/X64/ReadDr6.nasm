;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadDr6.Asm
;
; Abstract:
;
;   AsmReadDr6 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmReadDr6 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadDr6)
ASM_PFX(AsmReadDr6):
    mov     rax, dr6
    ret

