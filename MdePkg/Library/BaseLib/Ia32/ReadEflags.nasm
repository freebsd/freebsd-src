;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadEflags.Asm
;
; Abstract:
;
;   AsmReadEflags function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmReadEflags (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadEflags)
ASM_PFX(AsmReadEflags):
    pushfd
    pop     eax
    ret

