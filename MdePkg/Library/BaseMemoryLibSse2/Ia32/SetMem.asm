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
;   SetMem.asm
;
; Abstract:
;
;   SetMem function
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
;  InternalMemSetMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT8  Value
;    );
;------------------------------------------------------------------------------
InternalMemSetMem   PROC    USES    edi
    mov     edx, [esp + 12]             ; edx <- Count
    mov     edi, [esp + 8]              ; edi <- Buffer
    mov     al, [esp + 16]              ; al <- Value
    xor     ecx, ecx
    sub     ecx, edi
    and     ecx, 15                     ; ecx + edi aligns on 16-byte boundary
    jz      @F
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx
    rep     stosb
@@:
    mov     ecx, edx
    and     edx, 15
    shr     ecx, 4                      ; ecx <- # of DQwords to set
    jz      @SetBytes
    mov     ah, al                      ; ax <- Value | (Value << 8)
    add     esp, -16
    movdqu  [esp], xmm0                 ; save xmm0
    movd    xmm0, eax
    pshuflw xmm0, xmm0, 0               ; xmm0[0..63] <- Value repeats 8 times
    movlhps xmm0, xmm0                  ; xmm0 <- Value repeats 16 times
@@:
    movntdq [edi], xmm0                 ; edi should be 16-byte aligned
    add     edi, 16
    loop    @B
    mfence
    movdqu  xmm0, [esp]                 ; restore xmm0
    add     esp, 16                     ; stack cleanup
@SetBytes:
    mov     ecx, edx
    rep     stosb
    mov     eax, [esp + 8]              ; eax <- Buffer as return value
    ret
InternalMemSetMem   ENDP

    END
