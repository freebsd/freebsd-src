;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   DisableCache.Asm
;
; Abstract:
;
;   Set the CD bit of CR0 to 1, clear the NW bit of CR0 to 0, and flush all caches with a
;   WBINVD instruction.
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmDisableCache (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmDisableCache)
ASM_PFX(AsmDisableCache):
    mov     eax, cr0
    bts     eax, 30
    btr     eax, 29
    mov     cr0, eax
    wbinvd
    ret

