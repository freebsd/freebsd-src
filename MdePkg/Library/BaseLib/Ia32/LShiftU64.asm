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
;   LShiftU64.asm
;
; Abstract:
;
;   64-bit left shift function for IA-32
;
;------------------------------------------------------------------------------

    .686
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathLShiftU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
InternalMathLShiftU64   PROC
    mov     cl, [esp + 12]
    xor     eax, eax
    mov     edx, [esp + 4]
    test    cl, 32                      ; Count >= 32?
    jnz     @F
    mov     eax, edx
    mov     edx, [esp + 8]
@@:    
    shld    edx, eax, cl
    shl     eax, cl
    ret
InternalMathLShiftU64   ENDP

    END
