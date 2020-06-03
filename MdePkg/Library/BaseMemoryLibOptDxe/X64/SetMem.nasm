;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
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

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT8  Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem)
ASM_PFX(InternalMemSetMem):
    push    rdi
    push    rbx
    push    rcx       ; push Buffer
    mov     rax, r8   ; rax = Value
    and     rax, 0xff ; rax = lower 8 bits of r8, upper 56 bits are 0
    mov     ah,  al   ; ah  = al
    mov     bx,  ax   ; bx  = ax
    shl     rax, 0x10  ; rax = ax << 16
    mov     ax,  bx   ; ax  = bx
    mov     rbx, rax  ; ebx = eax
    shl     rax, 0x20  ; rax = rax << 32
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
    pop     rbx
    pop     rdi
    ret

