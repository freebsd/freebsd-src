;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   SetMem32.nasm
;
; Abstract:
;
;   SetMem32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem32 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT32 Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem32)
ASM_PFX(InternalMemSetMem32):
    movq    mm0, r8
    mov     rax, rcx                    ; rax <- Buffer
    xchg    rcx, rdx                    ; rcx <- Count  rdx <- Buffer
    shr     rcx, 1                      ; rcx <- # of qwords to set
    jz      @SetDwords
    pshufw  mm0, mm0, 44h
.0:
    movntq  [rdx], mm0
    lea     rdx, [rdx + 8]              ; use "lea" to avoid flag changes
    loop    .0
    mfence
@SetDwords:
    jnc     .1
    movd    [rdx], mm0
.1:
    ret

