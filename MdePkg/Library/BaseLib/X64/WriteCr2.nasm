;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   WriteCr2.Asm
;
; Abstract:
;
;   AsmWriteCr2 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmWriteCr2 (
;   UINTN  Cr2
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteCr2)
ASM_PFX(AsmWriteCr2):
    mov     cr2, rcx
    mov     rax, rcx
    ret

