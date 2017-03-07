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

    .686
    .model  flat,C
    .xmm
    .code

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem32 (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT32 Value
;    );
;------------------------------------------------------------------------------
InternalMemSetMem32 PROC    USES    edi
    mov     edx, [esp + 12]
    mov     edi, [esp + 8]
    xor     ecx, ecx
    sub     ecx, edi
    and     ecx, 15                     ; ecx + edi aligns on 16-byte boundary
    mov     eax, [esp + 16]
    jz      @F
    shr     ecx, 2
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx
    rep     stosd
@@:
    mov     ecx, edx
    and     edx, 3
    shr     ecx, 2
    jz      @SetDwords
    movd    xmm0, eax
    pshufd  xmm0, xmm0, 0
@@:
    movntdq [edi], xmm0
    add     edi, 16
    loop    @B
    mfence
@SetDwords:
    mov     ecx, edx
    rep     stosd
    mov     eax, [esp + 8]
    ret
InternalMemSetMem32 ENDP

    END
