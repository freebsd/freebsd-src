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
;   RRotU64.asm
;
; Abstract:
;
;   64-bit right rotation for Ia32
;
;------------------------------------------------------------------------------

    .686
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathRRotU64 (
;   IN      UINT64                    Operand,
;   IN      UINTN                     Count
;   );
;------------------------------------------------------------------------------
InternalMathRRotU64 PROC    USES    ebx
    mov     cl, [esp + 16]
    mov     eax, [esp + 8]
    mov     edx, [esp + 12]
    shrd    ebx, eax, cl
    shrd    eax, edx, cl
    rol     ebx, cl
    shrd    edx, ebx, cl
    test    cl, 32                      ; Count >= 32?
    jz      @F
    mov     ecx, eax                    ; switch eax & edx if Count >= 32
    mov     eax, edx
    mov     edx, ecx
@@:    
    ret
InternalMathRRotU64 ENDP

    END
