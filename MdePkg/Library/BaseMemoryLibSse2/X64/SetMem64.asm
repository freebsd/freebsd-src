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
;   SetMem64.asm
;
; Abstract:
;
;   SetMem64 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemSetMem64 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT64 Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem64 PROC
    mov     rax, rcx                    ; rax <- Buffer
    xchg    rcx, rdx                    ; rcx <- Count & rdx <- Buffer
    test    dl, 8
    movd    xmm0, r8
    jz      @F
    mov     [rdx], r8
    add     rdx, 8
    dec     rcx
@@:
    shr     rcx, 1
    jz      @SetQwords
    movlhps xmm0, xmm0
@@:
    movntdq [rdx], xmm0
    lea     rdx, [rdx + 16]
    loop    @B
    mfence
@SetQwords:
    jnc     @F
    mov     [rdx], r8
@@:
    ret
InternalMemSetMem64 ENDP

    END
