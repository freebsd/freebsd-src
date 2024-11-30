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
;
; NASM doesn't support the vmmcall instruction in 32-bit mode and NASM versions
; before 2.12 cannot translate the 64-bit "rep vmmcall" instruction into elf32
; format. Given that VMGEXIT does not make sense on IA32, provide a stub
; implementation that is identical to CpuBreakpoint(). In practice, AsmVmgExit()
; should never be called on IA32.
;
    int  3
    ret

