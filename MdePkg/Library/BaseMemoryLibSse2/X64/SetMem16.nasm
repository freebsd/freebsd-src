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
;   SetMem16.nasm
;
; Abstract:
;
;   SetMem16 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem16 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT16 Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem16)
ASM_PFX(InternalMemSetMem16):
    push    rdi
    mov     rdi, rcx
    mov     r9, rdi
    xor     rcx, rcx
    sub     rcx, rdi
    and     rcx, 15
    mov     rax, r8
    jz      .0
    shr     rcx, 1
    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx
    rep     stosw
.0:
    mov     rcx, rdx
    and     edx, 7
    shr     rcx, 3
    jz      @SetWords
    movd    xmm0, eax
    pshuflw xmm0, xmm0, 0
    movlhps xmm0, xmm0
.1:
    movntdq [rdi], xmm0
    add     rdi, 16
    loop    .1
    mfence
@SetWords:
    mov     ecx, edx
    rep     stosw
    mov     rax, r9
    pop     rdi
    ret

