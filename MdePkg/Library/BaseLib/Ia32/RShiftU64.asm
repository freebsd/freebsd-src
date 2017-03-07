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
;   RShiftU64.asm
;
; Abstract:
;
;   64-bit logical right shift function for IA-32
;
;------------------------------------------------------------------------------

    .686
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathRShiftU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
InternalMathRShiftU64   PROC
    mov     cl, [esp + 12]              ; cl <- Count
    xor     edx, edx
    mov     eax, [esp + 8]
    test    cl, 32                      ; Count >= 32?
    jnz     @F
    mov     edx, eax
    mov     eax, [esp + 4]
@@:    
    shrd    eax, edx, cl
    shr     edx, cl
    ret
InternalMathRShiftU64   ENDP

    END
