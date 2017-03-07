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
;   EnablePaging32.Asm
;
; Abstract:
;
;   AsmEnablePaging32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalX86EnablePaging32 (
;   IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
;   IN      VOID                      *Context1,    OPTIONAL
;   IN      VOID                      *Context2,    OPTIONAL
;   IN      VOID                      *NewStack
;   );
;------------------------------------------------------------------------------
InternalX86EnablePaging32 PROC
    mov     ebx, [esp + 4]
    mov     ecx, [esp + 8]
    mov     edx, [esp + 12]
    pushfd
    pop     edi                         ; save flags in edi
    cli
    mov     eax, cr0
    bts     eax, 31
    mov     esp, [esp + 16]
    mov     cr0, eax
    push    edi
    popfd                               ; restore flags
    push    edx
    push    ecx
    call    ebx
    jmp     $
InternalX86EnablePaging32 ENDP

    END
