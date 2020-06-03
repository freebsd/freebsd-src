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
    mov     eax, cr3
    mov     cr3, eax                    ; moving to CR3 flushes TLB
    ret

