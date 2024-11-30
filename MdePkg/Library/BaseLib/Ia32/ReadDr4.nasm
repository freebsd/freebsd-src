;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadDr4.Asm
;
; Abstract:
;
;   AsmReadDr4 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmReadDr4 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadDr4)
ASM_PFX(AsmReadDr4):
    ;
    ; DR4 is alias to DR6 only if DE (in CR4) is cleared. Otherwise, reading
    ; this register will cause a #UD exception.
    ;
    ; MS assembler doesn't support this instruction since no one would use it
    ; under normal circustances.
    ;
    mov     eax, dr4
    ret

