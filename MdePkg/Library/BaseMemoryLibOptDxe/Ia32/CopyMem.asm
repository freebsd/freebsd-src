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
    .xmm
    .code

;------------------------------------------------------------------------------
;  VOID *
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
    xor     ecx, ecx
    sub     ecx, edi
    and     ecx, 15                     ; ecx + edi aligns on 16-byte boundary
    jz      @F
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx                    ; edx <- remaining bytes to copy
    rep     movsb
@@:
    mov     ecx, edx
    and     edx, 15
    shr     ecx, 4                      ; ecx <- # of DQwords to copy
    jz      @CopyBytes
    add     esp, -16
    movdqu  [esp], xmm0                 ; save xmm0
@@:
    movdqu  xmm0, [esi]                 ; esi may not be 16-bytes aligned
    movntdq [edi], xmm0                 ; edi should be 16-bytes aligned
    add     esi, 16
    add     edi, 16
    loop    @B
    mfence
    movdqu  xmm0, [esp]                 ; restore xmm0
    add     esp, 16                     ; stack cleanup
    jmp     @CopyBytes
@CopyBackward:
    mov     esi, eax                    ; esi <- Last byte in Source
    lea     edi, [edi + edx - 1]        ; edi <- Last byte in Destination
    std
@CopyBytes:
    mov     ecx, edx
    rep     movsb
    cld
    mov     eax, [esp + 12]             ; eax <- Destination as return value
    ret
InternalMemCopyMem  ENDP

    END
