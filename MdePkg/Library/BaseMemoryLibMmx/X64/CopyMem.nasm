;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   CopyMem.nasm
;
; Abstract:
;
;   CopyMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID *
; EFIAPI
; InternalMemCopyMem (
;   OUT     VOID                      *DestinationBuffer,
;   IN      CONST VOID                *SourceBuffer,
;   IN      UINTN                     Length
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemCopyMem)
ASM_PFX(InternalMemCopyMem):
    push    rsi
    push    rdi
    mov     rsi, rdx                    ; rsi <- Source
    mov     rdi, rcx                    ; rdi <- Destination
    lea     r9, [rsi + r8 - 1]          ; r9 <- End of Source
    cmp     rsi, rdi
    mov     rax, rdi                    ; rax <- Destination as return value
    jae     .0
    cmp     r9, rdi
    jae     @CopyBackward               ; Copy backward if overlapped
.0:
    mov     rcx, r8
    and     r8, 7
    shr     rcx, 3                      ; rcx <- # of Qwords to copy
    jz      @CopyBytes
    movq    r10, mm0
.1:
    movq    mm0, [rsi]
    movntq  [rdi], mm0
    add     rsi, 8
    add     rdi, 8
    loop    .1
    mfence
    movq    mm0, r10
    jmp     @CopyBytes
@CopyBackward:
    mov     rsi, r9                     ; rsi <- End of Source
    lea     rdi, [rdi + r8 - 1]         ; rdi <- End of Destination
    std                                 ; set direction flag
@CopyBytes:
    mov     rcx, r8
    rep     movsb                       ; Copy bytes backward
    cld
    pop     rdi
    pop     rsi
    ret

