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
;   MultU64x32.asm
;
; Abstract:
;
;   Calculate the product of a 64-bit integer and a 32-bit integer
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathMultU64x32 (
;   IN      UINT64                    Multiplicand,
;   IN      UINT32                    Multiplier
;   );
;------------------------------------------------------------------------------
InternalMathMultU64x32  PROC
    mov     ecx, [esp + 12]
    mov     eax, ecx
    imul    ecx, [esp + 8]              ; overflow not detectable
    mul     dword ptr [esp + 4]
    add     edx, ecx
    ret
InternalMathMultU64x32  ENDP

    END
