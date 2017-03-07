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
;   DivU64x32.asm
;
; Abstract:
;
;   Calculate the quotient of a 64-bit integer by a 32-bit integer
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathDivU64x32 (
;   IN      UINT64                    Dividend,
;   IN      UINT32                    Divisor
;   );
;------------------------------------------------------------------------------
InternalMathDivU64x32   PROC
    mov     eax, [esp + 8]
    mov     ecx, [esp + 12]
    xor     edx, edx
    div     ecx
    push    eax                     ; save quotient on stack
    mov     eax, [esp + 8]
    div     ecx
    pop     edx                     ; restore high-order dword of the quotient
    ret
InternalMathDivU64x32   ENDP

    END
