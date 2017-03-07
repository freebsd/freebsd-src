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
;   WriteDr4.Asm
;
; Abstract:
;
;   AsmWriteDr4 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .586p
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmWriteDr4 (
;   IN UINTN Value
;   );
;------------------------------------------------------------------------------
AsmWriteDr4 PROC
    mov     eax, [esp + 4]
    ;
    ; DR4 is alias to DR6 only if DE (in CR4) is cleared. Otherwise, writing to
    ; this register will cause a #UD exception.
    ;
    ; MS assembler doesn't support this instruction since no one would use it
    ; under normal circustances. Here opcode is used.
    ;
    DB      0fh, 23h, 0e0h
    ret
AsmWriteDr4 ENDP

    END
