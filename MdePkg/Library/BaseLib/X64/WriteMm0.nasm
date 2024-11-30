;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   WriteMm0.Asm
;
; Abstract:
;
;   AsmWriteMm0 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmWriteMm0 (
;   IN UINT64   Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteMm0)
ASM_PFX(AsmWriteMm0):
    movq    mm0, rcx
    ret

