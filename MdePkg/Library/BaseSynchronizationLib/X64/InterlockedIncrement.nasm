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
;   InterlockedIncrement.Asm
;
; Abstract:
;
;   InterlockedIncrement function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; InternalSyncIncrement (
;   IN      volatile UINT32           *Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalSyncIncrement)
ASM_PFX(InternalSyncIncrement):
    lock    inc     dword [rcx]
    mov     eax, [rcx]
    ret

