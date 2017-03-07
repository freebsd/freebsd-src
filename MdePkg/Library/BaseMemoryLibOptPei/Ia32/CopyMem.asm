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
;   CopyMem.Asm
;
; Abstract:
;
;   CopyMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemCopyMem (
;    IN VOID   *Destination,
;    IN VOID   *Source,
;    IN UINTN  Count
;    )
;------------------------------------------------------------------------------
InternalMemCopyMem  PROC    USES    esi edi
    mov     esi, [esp+16]                ; esi <- Source
    mov     edi, [esp+12]                ; edi <- Destination
    mov     edx, [esp+20]                ; edx <- Count
    cmp     esi, edi
    je      @CopyDone
    cmp     edx, 0
    je      @CopyDone
    lea     eax, [esi + edx - 1]         ; eax <- End of Source
    cmp     esi, edi
    jae     @CopyBytes
    cmp     eax, edi
    jb      @CopyBytes                   ; Copy backward if overlapped
    mov     esi, eax                     ; esi <- End of Source
    lea     edi, [edi + edx - 1]         ; edi <- End of Destination
    std
@CopyBytes:
    mov     ecx, edx
    rep     movsb                        ; Copy bytes backward
    cld
@CopyDone:
    mov     eax, [esp + 12]             ; eax <- Destination as return value
    ret
InternalMemCopyMem  ENDP

    END
