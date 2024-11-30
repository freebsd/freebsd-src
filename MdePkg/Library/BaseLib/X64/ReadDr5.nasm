;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadDr5.Asm
;
; Abstract:
;
;   AsmReadDr5 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmReadDr5 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadDr5)
ASM_PFX(AsmReadDr5):
    ;
    ; There's no obvious reason to access this register, since it's aliased to
    ; DR7 when DE=0 or an exception generated when DE=1
    ;
    mov     rax, dr5
    ret

