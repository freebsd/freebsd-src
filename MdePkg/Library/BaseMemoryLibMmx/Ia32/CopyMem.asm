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
;   CopyMem.asm
;
; Abstract:
;
;   CopyMem function
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
;  InternalMemCopyMem (
;    IN VOID   *Destination,
;    IN VOID   *Source,
;    IN UINTN  Count
;    );
;------------------------------------------------------------------------------
InternalMemCopyMem  PROC    USES    esi edi
    mov     esi, [esp + 16]             ; esi <- Source
    mov     edi, [esp + 12]             ; edi <- Destination
    mov     edx, [esp + 20]             ; edx <- Count
    lea     eax, [esi + edx - 1]        ; eax <- End of Source
    cmp     esi, edi
    jae     @F
    cmp     eax, edi                    ; Overlapped?
    jae     @CopyBackward               ; Copy backward if overlapped
@@:
    mov     ecx, edx
    and     edx, 7
    shr     ecx, 3                      ; ecx <- # of Qwords to copy
    jz      @CopyBytes
    push    eax
    push    eax
    movq    [esp], mm0                  ; save mm0
@@:
    movq    mm0, [esi]
    movq    [edi], mm0
    add     esi, 8
    add     edi, 8
    loop    @B
    movq    mm0, [esp]                  ; restore mm0
    pop     ecx                         ; stack cleanup
    pop     ecx                         ; stack cleanup
    jmp     @CopyBytes
@CopyBackward:
    mov     esi, eax                    ; esi <- Last byte in Source
    lea     edi, [edi + edx - 1]        ; edi <- Last byte in Destination
    std
@CopyBytes:
    mov     ecx, edx
    rep     movsb
    cld
    mov     eax, [esp + 12]
    ret
InternalMemCopyMem  ENDP

    END
