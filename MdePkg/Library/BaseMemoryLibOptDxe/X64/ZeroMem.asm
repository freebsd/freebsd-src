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
;   ZeroMem.Asm
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
    push    rcx       ; push Buffer
    xor     rax, rax  ; rax = 0
    mov     rdi, rcx  ; rdi = Buffer
    mov     rcx, rdx  ; rcx = Count
    shr     rcx, 3    ; rcx = rcx / 8
    and     rdx, 7    ; rdx = rdx & 7
    cld
    rep     stosq
    mov     rcx, rdx  ; rcx = rdx
    rep     stosb
    pop     rax       ; rax = Buffer
    ret
InternalMemZeroMem  ENDP

    END
