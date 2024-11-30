;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   EnablePaging64.Asm
;
; Abstract:
;
;   AsmEnablePaging64 function
;
; Notes:
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalX86EnablePaging64 (
;   IN      UINT16                    Cs,
;   IN      UINT64                    EntryPoint,
;   IN      UINT64                    Context1,  OPTIONAL
;   IN      UINT64                    Context2,  OPTIONAL
;   IN      UINT64                    NewStack
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalX86EnablePaging64)
ASM_PFX(InternalX86EnablePaging64):
    cli
    mov     DWORD [esp], .0         ; offset for far retf, seg is the 1st arg
    mov     eax, cr4
    or      al, (1 << 5)
    mov     cr4, eax                    ; enable PAE
    mov     ecx, 0xc0000080
    rdmsr
    or      ah, 1                       ; set LME
    wrmsr
    mov     eax, cr0
    bts     eax, 31                     ; set PG
    mov     cr0, eax                    ; enable paging
    retf                                ; topmost 2 dwords hold the address
.0:
BITS 64
    mov     rbx, [esp]
    mov     rcx, [esp + 8]
    mov     rdx, [esp + 0x10]
    mov     rsp, [esp + 0x18]
    add     rsp, -0x20
    call    rbx
    hlt                                 ; no one should get here

