;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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

    .686p
    .model  flat,C
    .code

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
InternalX86EnablePaging64 PROC
    cli
    mov     DWORD PTR [esp], @F         ; offset for far retf, seg is the 1st arg
    mov     eax, cr4
    or      al, (1 SHL 5)
    mov     cr4, eax                    ; enable PAE
    mov     ecx, 0c0000080h
    rdmsr
    or      ah, 1                       ; set LME
    wrmsr
    mov     eax, cr0
    bts     eax, 31                     ; set PG
    mov     cr0, eax                    ; enable paging
    retf                                ; topmost 2 dwords hold the address
@@:                                     ; long mode starts here
    DB      67h, 48h                    ; 32-bit address size, 64-bit operand size
    mov     ebx, [esp]                  ; mov rbx, [esp]
    DB      67h, 48h
    mov     ecx, [esp + 8]              ; mov rcx, [esp + 8]
    DB      67h, 48h
    mov     edx, [esp + 10h]            ; mov rdx, [esp + 10h]
    DB      67h, 48h
    mov     esp, [esp + 18h]            ; mov rsp, [esp + 18h]
    DB      48h
    add     esp, -20h                   ; add rsp, -20h
    call    ebx                         ; call rbx
    hlt                                 ; no one should get here
InternalX86EnablePaging64 ENDP

    END
