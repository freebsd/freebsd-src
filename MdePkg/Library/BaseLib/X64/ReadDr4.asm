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
;   ReadDr4.Asm
;
; Abstract:
;
;   AsmReadDr4 function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmReadDr4 (
;   VOID
;   );
;------------------------------------------------------------------------------
AsmReadDr4  PROC
    ;
    ; There's no obvious reason to access this register, since it's aliased to
    ; DR7 when DE=0 or an exception generated when DE=1
    ;
    DB      0fh, 21h, 0e0h
    ret
AsmReadDr4  ENDP

    END
