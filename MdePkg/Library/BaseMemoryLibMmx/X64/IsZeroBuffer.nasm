;------------------------------------------------------------------------------
;
; Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   IsZeroBuffer.nasm
;
; Abstract:
;
;   IsZeroBuffer function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  BOOLEAN
;  EFIAPI
;  InternalMemIsZeroBuffer (
;    IN CONST VOID  *Buffer,
;    IN UINTN       Length
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemIsZeroBuffer)
ASM_PFX(InternalMemIsZeroBuffer):
    push    rdi
    mov     rdi, rcx                   ; rdi <- Buffer
    mov     rcx, rdx                   ; rcx <- Length
    shr     rcx, 3                     ; rcx <- number of qwords
    and     rdx, 7                     ; rdx <- number of trailing bytes
    xor     rax, rax                   ; rax <- 0, also set ZF
    repe    scasq
    jnz     @ReturnFalse               ; ZF=0 means non-zero element found
    mov     rcx, rdx
    repe    scasb
    jnz     @ReturnFalse
    pop     rdi
    mov     rax, 1                     ; return TRUE
    ret
@ReturnFalse:
    pop     rdi
    xor     rax, rax
    ret                                ; return FALSE

