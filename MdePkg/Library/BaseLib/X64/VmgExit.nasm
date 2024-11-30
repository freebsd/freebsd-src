;------------------------------------------------------------------------------
;
; Copyright (C) 2020, Advanced Micro Devices, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   VmgExit.Asm
;
; Abstract:
;
;   AsmVmgExit function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; AsmVmgExit (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmVmgExit)
ASM_PFX(AsmVmgExit):
    rep     vmmcall
    ret

