;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   WriteDr4.Asm
;
; Abstract:
;
;   AsmWriteDr4 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmWriteDr4 (
;   IN UINTN Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteDr4)
ASM_PFX(AsmWriteDr4):
    ;
    ; There's no obvious reason to access this register, since it's aliased to
    ; DR6 when DE=0 or an exception generated when DE=1
    ;
    mov     dr4, rcx
    mov     rax, rcx
    ret

