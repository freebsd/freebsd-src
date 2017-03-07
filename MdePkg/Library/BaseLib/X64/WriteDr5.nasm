;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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
;   WriteDr5.Asm
;
; Abstract:
;
;   AsmWriteDr5 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINTN
; EFIAPI
; AsmWriteDr5 (
;   IN UINTN Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmWriteDr5)
ASM_PFX(AsmWriteDr5):
    ;
    ; There's no obvious reason to access this register, since it's aliased to
    ; DR7 when DE=0 or an exception generated when DE=1
    ;
    DB      0xf, 0x23, 0xe9
    mov     rax, rcx
    ret

