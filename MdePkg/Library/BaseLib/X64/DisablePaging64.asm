;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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
;   DisablePaging64.Asm
;
; Abstract:
;
;   AsmDisablePaging64 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalX86DisablePaging64 (
;   IN      UINT16                    Cs,
;   IN      UINT32                    EntryPoint,
;   IN      UINT32                    Context1,  OPTIONAL
;   IN      UINT32                    Context2,  OPTIONAL
;   IN      UINT32                    NewStack
;   );
;------------------------------------------------------------------------------
InternalX86DisablePaging64    PROC
    cli
    lea     rsi, @F                     ; rsi <- The start address of transition code
    mov     edi, [rsp + 28h]            ; rdi <- New stack
    lea     rax, mTransitionEnd         ; rax <- end of transition code
    sub     rax, rsi                    ; rax <- The size of transition piece code 
    add     rax, 4                      ; Round RAX up to the next 4 byte boundary
    and     al, 0fch
    sub     rdi, rax                    ; rdi <- Use stack to hold transition code
    mov     r10d, edi                   ; r10 <- The start address of transicition code below 4G
    push    rcx                         ; save rcx to stack
    mov     rcx, rax                    ; rcx <- The size of transition piece code
    rep     movsb                       ; copy transition code to top of new stack which must be below 4GB
    pop     rcx                         ; restore rcx
    
    mov     esi, r8d
    mov     edi, r9d
    mov     eax, r10d                   ; eax <- start of the transition code on the stack
    sub     eax, 4                      ; eax <- One slot below transition code on the stack
    push    rcx                         ; push Cs to stack
    push    r10                         ; push address of tansition code on stack
    DB      48h                         ; prefix to composite "retq" with next "retf"
    retf                                ; Use far return to load CS register from stack

; Start of transition code
@@:
    mov     esp, eax                    ; set up new stack
    mov     rax, cr0
    btr     eax, 31                     ; Clear CR0.PG
    mov     cr0, rax                    ; disable paging and caches
    
    mov     ebx, edx                    ; save EntryPoint to rbx, for rdmsr will overwrite rdx
    mov     ecx, 0c0000080h
    rdmsr
    and     ah, NOT 1                   ; clear LME
    wrmsr
    mov     rax, cr4
    and     al, NOT (1 SHL 5)           ; clear PAE
    mov     cr4, rax
    push    rdi                         ; push Context2
    push    rsi                         ; push Context1
    call    rbx                         ; transfer control to EntryPoint
    hlt                                 ; no one should get here
InternalX86DisablePaging64    ENDP

mTransitionEnd LABEL    BYTE

    END
