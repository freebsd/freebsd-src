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
;   ReadIdtr.Asm
;
; Abstract:
;
;   AsmReadIdtr function
;
; Notes:
;
;------------------------------------------------------------------------------

    .386
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalX86ReadIdtr (
;   OUT     IA32_DESCRIPTOR           *Idtr
;   );
;------------------------------------------------------------------------------
InternalX86ReadIdtr PROC
    mov     eax, [esp + 4]
    sidt    fword ptr [eax]
    ret
InternalX86ReadIdtr ENDP

    END
