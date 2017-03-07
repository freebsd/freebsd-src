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
;   CpuId.Asm
;
; Abstract:
;
;   AsmCpuid function
;
; Notes:
;
;------------------------------------------------------------------------------

    .586
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; InternalMathSwapBytes64 (
;   IN      UINT64                    Operand
;   );
;------------------------------------------------------------------------------
InternalMathSwapBytes64 PROC
    mov     eax, [esp + 8]              ; eax <- upper 32 bits
    mov     edx, [esp + 4]              ; edx <- lower 32 bits
    bswap   eax
    bswap   edx
    ret
InternalMathSwapBytes64 ENDP

    END
