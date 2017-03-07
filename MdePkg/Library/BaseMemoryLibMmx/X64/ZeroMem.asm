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
;    );
;------------------------------------------------------------------------------
InternalMemZeroMem  PROC    USES    rdi
    mov     rdi, rcx
    mov     rcx, rdx
    mov     r8, rdi
    and     edx, 7
    shr     rcx, 3
    jz      @ZeroBytes
    DB      0fh, 0efh, 0c0h             ; pxor mm0, mm0
@@:
    DB      0fh, 0e7h, 7                ; movntq [rdi], mm0
    add     rdi, 8
    loop    @B
    DB      0fh, 0aeh, 0f0h             ; mfence
@ZeroBytes:
    xor     eax, eax
    mov     ecx, edx
    rep     stosb
    mov     rax, r8
    ret
InternalMemZeroMem  ENDP

    END
