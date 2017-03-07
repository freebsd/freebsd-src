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
;   SetMem.Asm
;
; Abstract:
;
;   SetMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT8  Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem   PROC    USES    rdi rbx
    push    rcx       ; push Buffer
    mov     rax, r8   ; rax = Value
    and     rax, 0ffh ; rax = lower 8 bits of r8, upper 56 bits are 0
    mov     ah,  al   ; ah  = al
    mov     bx,  ax   ; bx  = ax
    shl     rax, 10h  ; rax = ax << 16
    mov     ax,  bx   ; ax  = bx
    mov     rbx, rax  ; ebx = eax
    shl     rax, 20h  ; rax = rax << 32
    or      rax, rbx  ; eax = ebx
    mov     rdi, rcx  ; rdi = Buffer
    mov     rcx, rdx  ; rcx = Count
    shr     rcx, 3    ; rcx = rcx / 8
    cld
    rep     stosq 
    mov     rcx, rdx  ; rcx = rdx
    and     rcx, 7    ; rcx = rcx & 7
    rep     stosb 
    pop     rax       ; rax = Buffer
    ret
InternalMemSetMem   ENDP

    END
