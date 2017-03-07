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
;   DivError.asm
;
; Abstract:
;
;   Set error flag for all division functions
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathDivRemU64x32 (
;   IN      UINT64                    Dividend,
;   IN      UINT32                    Divisor,
;   OUT     UINT32                    *Remainder
;   );
;------------------------------------------------------------------------------
InternalMathDivRemU64x32    PROC
    mov     ecx, [esp + 12]         ; ecx <- divisor
    mov     eax, [esp + 8]          ; eax <- dividend[32..63]
    xor     edx, edx
    div     ecx                     ; eax <- quotient[32..63], edx <- remainder
    push    eax
    mov     eax, [esp + 8]          ; eax <- dividend[0..31]
    div     ecx                     ; eax <- quotient[0..31]
    mov     ecx, [esp + 20]         ; ecx <- Remainder
    jecxz   @F                      ; abandon remainder if Remainder == NULL
    mov     [ecx], edx
@@:
    pop     edx                     ; edx <- quotient[32..63]
    ret
InternalMathDivRemU64x32    ENDP

    END
