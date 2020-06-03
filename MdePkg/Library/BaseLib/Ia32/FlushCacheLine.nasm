;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   FlushCacheLine.Asm
;
; Abstract:
;
;   AsmFlushCacheLine function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; VOID *
; EFIAPI
; AsmFlushCacheLine (
;   IN      VOID                      *LinearAddress
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmFlushCacheLine)
ASM_PFX(AsmFlushCacheLine):
    ;
    ; If the CPU does not support CLFLUSH instruction,
    ; then promote flush range to flush entire cache.
    ;
    mov     eax, 1
    push    ebx
    cpuid
    pop     ebx
    mov     eax, [esp + 4]
    test    edx, BIT19
    jz      .0
    clflush [eax]
    ret
.0:
    wbinvd
    ret

