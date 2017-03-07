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
;   FxSave.Asm
;
; Abstract:
;
;   AsmFxSave function
;
; Notes:
;
;------------------------------------------------------------------------------

    .586
    .model  flat,C
    .xmm
    .code

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalX86FxSave (
;   OUT IA32_FX_BUFFER *Buffer
;   );
;------------------------------------------------------------------------------
InternalX86FxSave PROC
    mov     eax, [esp + 4]              ; Buffer must be 16-byte aligned
    fxsave  [eax]
    ret
InternalX86FxSave ENDP

    END
