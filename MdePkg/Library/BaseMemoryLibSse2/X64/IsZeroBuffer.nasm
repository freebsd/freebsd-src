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
    push         rdi
    mov          rdi, rcx              ; rdi <- Buffer
    xor          rcx, rcx              ; rcx <- 0
    sub          rcx, rdi
    and          rcx, 15               ; rcx + rdi aligns on 16-byte boundary
    jz           @Is16BytesZero
    cmp          rcx, rdx              ; Length already in rdx
    cmova        rcx, rdx              ; bytes before the 16-byte boundary
    sub          rdx, rcx
    xor          rax, rax              ; rax <- 0, also set ZF
    repe         scasb
    jnz          @ReturnFalse          ; ZF=0 means non-zero element found
@Is16BytesZero:
    mov          rcx, rdx
    and          rdx, 15
    shr          rcx, 4
    jz           @IsBytesZero
.0:
    pxor         xmm0, xmm0            ; xmm0 <- 0
    pcmpeqb      xmm0, [rdi]           ; check zero for 16 bytes
    pmovmskb     eax, xmm0             ; eax <- compare results
                                       ; nasm doesn't support 64-bit destination
                                       ; for pmovmskb
    cmp          eax, 0xffff
    jnz          @ReturnFalse
    add          rdi, 16
    loop         .0
@IsBytesZero:
    mov          rcx, rdx
    xor          rax, rax              ; rax <- 0, also set ZF
    repe         scasb
    jnz          @ReturnFalse          ; ZF=0 means non-zero element found
    pop          rdi
    mov          rax, 1                ; return TRUE
    ret
@ReturnFalse:
    pop          rdi
    xor          rax, rax
    ret                                ; return FALSE

