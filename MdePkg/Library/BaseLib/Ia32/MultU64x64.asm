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
;   MultU64x64.asm
;
; Abstract:
;
;   Calculate the product of a 64-bit integer and another 64-bit integer
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathMultU64x64 (
;   IN      UINT64                    Multiplicand,
;   IN      UINT64                    Multiplier
;   );
;------------------------------------------------------------------------------
InternalMathMultU64x64  PROC    USES    ebx
    mov     ebx, [esp + 8]              ; ebx <- M1[0..31]
    mov     edx, [esp + 16]             ; edx <- M2[0..31]
    mov     ecx, ebx
    mov     eax, edx
    imul    ebx, [esp + 20]             ; ebx <- M1[0..31] * M2[32..63]
    imul    edx, [esp + 12]             ; edx <- M1[32..63] * M2[0..31]
    add     ebx, edx                    ; carries are abandoned
    mul     ecx                         ; edx:eax <- M1[0..31] * M2[0..31]
    add     edx, ebx                    ; carries are abandoned
    ret
InternalMathMultU64x64  ENDP

    END
