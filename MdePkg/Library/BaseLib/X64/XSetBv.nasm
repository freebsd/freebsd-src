;------------------------------------------------------------------------------
;
; Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   XSetBv.nasm
;
; Abstract:
;
;   AsmXSetBv function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmXSetBv (
;   IN UINT32  Index,
;   IN UINT64  Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmXSetBv)
ASM_PFX(AsmXSetBv):
    mov     rax, rdx                    ; meanwhile, rax <- return value
    shr     rdx, 0x20                    ; edx:eax contains the value to write
    xsetbv
    ret
