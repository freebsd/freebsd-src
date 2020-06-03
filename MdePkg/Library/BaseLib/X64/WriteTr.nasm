;------------------------------------------------------------------------------ ;
; Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   WriteTr.nasm
;
; Abstract:
;
;   Write TR register
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; AsmWriteTr (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteTr)
ASM_PFX(AsmWriteTr):
    mov     eax, ecx
    ltr     ax
    ret

