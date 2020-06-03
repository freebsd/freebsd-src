;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ReadMm0.Asm
;
; Abstract:
;
;   AsmReadMm0 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm0 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm0)
ASM_PFX(AsmReadMm0):
    ;
    ; 64-bit MASM doesn't support MMX instructions, so use opcode here
    ;
    DB      0x48, 0xf, 0x7e, 0xc0
    ret

