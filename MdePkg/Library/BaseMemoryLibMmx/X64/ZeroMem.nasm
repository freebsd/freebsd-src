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
;   ZeroMem.nasm
;
; Abstract:
;
;   ZeroMem function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;  VOID *
;  InternalMemZeroMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemZeroMem)
ASM_PFX(InternalMemZeroMem):
    push    rdi
    mov     rdi, rcx
    mov     rcx, rdx
    mov     r8, rdi
    and     edx, 7
    shr     rcx, 3
    jz      @ZeroBytes
    DB      0xf, 0xef, 0xc0             ; pxor mm0, mm0
.0:
    DB      0xf, 0xe7, 7                ; movntq [rdi], mm0
    add     rdi, 8
    loop    .0
    DB      0xf, 0xae, 0xf0             ; mfence
@ZeroBytes:
    xor     eax, eax
    mov     ecx, edx
    rep     stosb
    mov     rax, r8
    pop     rdi
    ret

