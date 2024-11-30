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
    ; DR5 is alias to DR7 only if DE (in CR4) is cleared. Otherwise, reading
    ; this register will cause a #UD exception.
    ;
    ; MS assembler doesn't support this instruction since no one would use it
    ; under normal circustances.
    ;
    mov     eax, dr5
    ret

