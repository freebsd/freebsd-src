;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   EnableCache.Asm
;
; Abstract:
;
;  Flush all caches with a WBINVD instruction, clear the CD bit of CR0 to 0, and clear
;  the NW bit of CR0 to 0
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmEnableCache (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmEnableCache)
ASM_PFX(AsmEnableCache):
    wbinvd
    mov     rax, cr0
    btr     rax, 29
    btr     rax, 30
    mov     cr0, rax
    ret

