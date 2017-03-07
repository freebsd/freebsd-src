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
;   SetMem32.asm
;
; Abstract:
;
;   SetMem32 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem32 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT8  Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem32 PROC    USES    rdi
    mov     rdi, rcx
    mov     r9, rdi
    xor     rcx, rcx
    sub     rcx, rdi
    and     rcx, 15
    mov     rax, r8
    jz      @F
    shr     rcx, 2
    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx
    rep     stosd
@@:
    mov     rcx, rdx
    and     edx, 3
    shr     rcx, 2
    jz      @SetDwords
    movd    xmm0, eax
    pshufd  xmm0, xmm0, 0
@@:
    movntdq [rdi], xmm0
    add     rdi, 16
    loop    @B
    mfence
@SetDwords:
    mov     ecx, edx
    rep     stosd
    mov     rax, r9
    ret
InternalMemSetMem32 ENDP

    END
