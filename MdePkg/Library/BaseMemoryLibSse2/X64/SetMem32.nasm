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
;    IN UINT8  Value
;    )
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemSetMem32)
ASM_PFX(InternalMemSetMem32):
    push    rdi
    mov     rdi, rcx
    mov     r9, rdi
    xor     rcx, rcx
    sub     rcx, rdi
    and     rcx, 15
    mov     rax, r8
    jz      .0
    shr     rcx, 2
    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx
    rep     stosd
.0:
    mov     rcx, rdx
    and     edx, 3
    shr     rcx, 2
    jz      @SetDwords
    movd    xmm0, eax
    pshufd  xmm0, xmm0, 0
.1:
    movntdq [rdi], xmm0
    add     rdi, 16
    loop    .1
    mfence
@SetDwords:
    mov     ecx, edx
    rep     stosd
    mov     rax, r9
    pop     rdi
    ret

