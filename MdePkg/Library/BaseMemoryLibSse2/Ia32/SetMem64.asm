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

    .686
    .model  flat,C
    .xmm
    .code

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem64 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT64 Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem64 PROC
    mov     eax, [esp + 4]              ; eax <- Buffer
    mov     ecx, [esp + 8]              ; ecx <- Count
    test    al, 8
    mov     edx, eax
    movq    xmm0, qword ptr [esp + 12]
    jz      @F
    movq    qword ptr [edx], xmm0
    add     edx, 8
    dec     ecx
@@:
    shr     ecx, 1
    jz      @SetQwords
    movlhps xmm0, xmm0
@@:
    movntdq [edx], xmm0
    lea     edx, [edx + 16]
    loop    @B
    mfence
@SetQwords:
    jnc     @F
    movq    qword ptr [edx], xmm0
@@:
    ret
InternalMemSetMem64 ENDP

    END
