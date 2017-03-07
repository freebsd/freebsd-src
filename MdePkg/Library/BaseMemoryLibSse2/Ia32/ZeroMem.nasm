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
;  EFIAPI
;  InternalMemZeroMem (
;    IN VOID   *Buffer,
;    IN UINTN  Count
;    );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMemZeroMem)
ASM_PFX(InternalMemZeroMem):
    push    edi
    mov     edi, [esp + 8]
    mov     edx, [esp + 12]
    xor     ecx, ecx
    sub     ecx, edi
    xor     eax, eax
    and     ecx, 15
    jz      .0
    cmp     ecx, edx
    cmova   ecx, edx
    sub     edx, ecx
    rep     stosb
.0:
    mov     ecx, edx
    and     edx, 15
    shr     ecx, 4
    jz      @ZeroBytes
    pxor    xmm0, xmm0
.1:
    movntdq [edi], xmm0
    add     edi, 16
    loop    .1
    mfence
@ZeroBytes:
    mov     ecx, edx
    rep     stosb
    mov     eax, [esp + 8]
    pop     edi
    ret

