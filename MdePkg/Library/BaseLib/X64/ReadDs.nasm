;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadDs.Asm
;
; Abstract:
;
;   AsmReadDs function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT16
; EFIAPI
; AsmReadDs (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadDs)
ASM_PFX(AsmReadDs):
    mov     eax, ds
    ret

