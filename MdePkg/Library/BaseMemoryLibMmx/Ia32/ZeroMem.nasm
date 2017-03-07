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
    push    edi
    mov     edi, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edx, ecx
    shr     ecx, 3
    jz      @ZeroBytes
    pxor    mm0, mm0
.0:
    movq    [edi], mm0
    add     edi, 8
    loop    .0
@ZeroBytes:
    and     edx, 7
    xor     eax, eax
    mov     ecx, edx
    rep     stosb
    mov     eax, [esp + 8]
    pop     edi
    ret

