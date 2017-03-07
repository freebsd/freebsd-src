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

    .code

;------------------------------------------------------------------------------
; VOID *
; EFIAPI
; InternalMemSetMem (
;   OUT     VOID                      *Buffer,
;   IN      UINTN                     Length,
;   IN      UINT8                     Value
;   );
;------------------------------------------------------------------------------
InternalMemSetMem   PROC    USES    rdi
    mov     rax, r8
    mov     ah, al
    DB      48h, 0fh, 6eh, 0c0h         ; movd mm0, rax
    mov     r8, rcx
    mov     rdi, r8                     ; rdi <- Buffer
    mov     rcx, rdx
    and     edx, 7
    shr     rcx, 3
    jz      @SetBytes
    DB      0fh, 70h, 0C0h, 00h         ; pshufw mm0, mm0, 0h
@@:
    DB      0fh, 0e7h, 07h              ; movntq [rdi], mm0
    add     rdi, 8
    loop    @B
    mfence
@SetBytes:
    mov     ecx, edx
    rep     stosb
    mov     rax, r8
    ret
InternalMemSetMem   ENDP

    END
