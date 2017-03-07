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
;   ZeroMem.asm
;
; Abstract:
;
;   ZeroMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemZeroMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count
;    )
;------------------------------------------------------------------------------
InternalMemZeroMem  PROC    USES    rdi
    mov     rdi, rcx
    xor     rcx, rcx
    xor     eax, eax
    sub     rcx, rdi
    and     rcx, 15
    mov     r8, rdi
    jz      @F
    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx
    rep     stosb
@@:
    mov     rcx, rdx
    and     edx, 15
    shr     rcx, 4
    jz      @ZeroBytes
    pxor    xmm0, xmm0
@@:
    movntdq [rdi], xmm0                 ; rdi should be 16-byte aligned
    add     rdi, 16
    loop    @B
    mfence
@ZeroBytes:
    mov     ecx, edx
    rep     stosb
    mov     rax, r8
    ret
InternalMemZeroMem  ENDP

    END
