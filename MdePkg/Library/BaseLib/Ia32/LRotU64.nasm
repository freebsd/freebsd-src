;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
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
;   LRotU64.nasm
;
; Abstract:
;
;   64-bit left rotation for Ia32
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathLRotU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathLRotU64)
ASM_PFX(InternalMathLRotU64):
    push    ebx
    mov     cl, [esp + 16]
    mov     edx, [esp + 12]
    mov     eax, [esp + 8]
    shld    ebx, edx, cl
    shld    edx, eax, cl
    ror     ebx, cl
    shld    eax, ebx, cl
    test    cl, 32                      ; Count >= 32?
    jz      .0
    mov     ecx, eax
    mov     eax, edx
    mov     edx, ecx
.0:
    pop     ebx
    ret

