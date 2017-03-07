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
;   LShiftU64.nasm
;
; Abstract:
;
;   64-bit left shift function for IA-32
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathLShiftU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalMathLShiftU64)
ASM_PFX(InternalMathLShiftU64):
    mov     cl, [esp + 12]
    xor     eax, eax
    mov     edx, [esp + 4]
    test    cl, 32                      ; Count >= 32?
    jnz     .0
    mov     eax, edx
    mov     edx, [esp + 8]
.0:
    shld    edx, eax, cl
    shl     eax, cl
    ret

