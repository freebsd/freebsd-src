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
    .mmx
    .code

;------------------------------------------------------------------------------
;  VOID *
;  EFIAPI
;  InternalMemSetMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count,
;    IN UINT8  Value
;    )
;------------------------------------------------------------------------------
InternalMemSetMem   PROC    USES    edi
    mov     al, [esp + 16]
    mov     ah, al
    shrd    edx, eax, 16
    shld    eax, edx, 16
    mov     ecx, [esp + 12]             ; ecx <- Count
    mov     edi, [esp + 8]              ; edi <- Buffer
    mov     edx, ecx
    and     edx, 7
    shr     ecx, 3                      ; # of Qwords to set
    jz      @SetBytes
    add     esp, -10h
    movq    [esp], mm0                  ; save mm0
    movq    [esp + 8], mm1              ; save mm1
    movd    mm0, eax
    movd    mm1, eax
    psllq   mm0, 32
    por     mm0, mm1                    ; fill mm0 with 8 Value's
@@:
    movq    [edi], mm0
    add     edi, 8
    loop    @B
    movq    mm0, [esp]                  ; restore mm0
    movq    mm1, [esp + 8]              ; restore mm1
    add     esp, 10h                    ; stack cleanup
@SetBytes:
    mov     ecx, edx
    rep     stosb
    mov     eax, [esp + 8]              ; eax <- Buffer as return value
    ret
InternalMemSetMem   ENDP

    END
