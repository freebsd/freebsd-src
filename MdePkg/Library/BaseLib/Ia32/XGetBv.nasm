;------------------------------------------------------------------------------
;
; Copyright (C) 2020, Advanced Micro Devices, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   XGetBv.Asm
;
; Abstract:
;
;   AsmXgetBv function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmXGetBv (
;   IN UINT32  Index
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmXGetBv)
ASM_PFX(AsmXGetBv):
    mov     ecx, [esp + 4]
    xgetbv
    ret
