;------------------------------------------------------------------------------ ;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   CpuFlushTlb.Asm
;
; Abstract:
;
;   CpuFlushTlb function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; CpuFlushTlb (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(CpuFlushTlb)
ASM_PFX(CpuFlushTlb):
    mov     rax, cr3
    mov     cr3, rax
    ret

