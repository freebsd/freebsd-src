;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
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
;   InterlockedDecrement.Asm
;
; Abstract:
;
;   InterlockedDecrement function
;
; Notes:
;
;------------------------------------------------------------------------------

    .code

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; InternalSyncDecrement (
;   IN      volatile UINT32           *Value
;   );
;------------------------------------------------------------------------------
InternalSyncDecrement   PROC
    lock    dec     dword ptr [rcx]
    mov     eax, [rcx]
    ret
InternalSyncDecrement   ENDP

    END
