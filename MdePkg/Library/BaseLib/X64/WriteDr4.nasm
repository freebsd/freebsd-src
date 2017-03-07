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

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmWriteDr4 (
;   IN UINTN Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteDr4)
ASM_PFX(AsmWriteDr4):
    ;
    ; There's no obvious reason to access this register, since it's aliased to
    ; DR6 when DE=0 or an exception generated when DE=1
    ;
    DB      0xf, 0x23, 0xe1
    mov     rax, rcx
    ret

